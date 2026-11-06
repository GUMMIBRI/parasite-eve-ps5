// SPDX-License-Identifier: MIT
// This local template is copied into the export as generate-build16-shaders.cjs.
const fs=require('fs'),p=require('path'),cp=require('child_process'),c=require('crypto');
module.exports=function(root,compiler){
 const own=__dirname,m=require('./build16-upgrade/manifest.json'),sha=b=>c.createHash('sha256').update(b).digest('hex'),out=p.join(root,'build-shaders');
 if(sha(fs.readFileSync(compiler))!==m.shaderCompilerSha256)throw Error('Pinned shader compiler mismatch');
 let header=fs.readFileSync(p.join(own,'build16-upgrade',m.generatedHeader.template),'utf8');
 if(sha(header)!==m.generatedHeader.templateSha256)throw Error('Filter header style mismatch');
 fs.mkdirSync(out,{recursive:true});const records=[];
 for(const x of m.shaderModules){
  const source=p.join(root,x.source),target=p.join(out,'display-filter.'+x.stage+'.spv');
  if(sha(fs.readFileSync(source))!==x.sourceSha256)throw Error('Filter shader source mismatch '+x.source);
  const args=['-V','--target-env','vulkan1.0','-S',x.stage,source,'-o',target],q=cp.spawnSync(compiler,args,{windowsHide:true,encoding:'utf8'});
  if(q.status!==0)throw Error('Filter shader compile failed '+x.stage+'\n'+q.stdout+'\n'+q.stderr);
  const bytes=fs.readFileSync(target);if(bytes.length!==x.bytes||sha(bytes)!==x.spirvSha256)throw Error('Filter SPIR-V mismatch '+x.stage);
  const rows=[];for(let i=0;i<bytes.length;i+=16)rows.push('    '+[...bytes.subarray(i,i+16)].map(v=>'0x'+v.toString(16).padStart(2,'0')).join(','));
  header=header.replace('{{'+x.stage.toUpperCase()+'_BYTES}}','\n'+rows.join(',\n')+'\n');records.push({stage:x.stage,sourceSha256:x.sourceSha256,spirvSha256:sha(bytes),bytes:bytes.length});
 }
 if(sha(header)!==m.generatedHeader.after)throw Error('Generated selected filter header mismatch');
 fs.writeFileSync(p.join(root,m.generatedHeader.path),header);
 fs.writeFileSync(p.join(root,'DISPLAY-FILTER-SHADER-PROVENANCE.json'),JSON.stringify({compilerSha256:m.shaderCompilerSha256,records,headerSha256:sha(header),selectedHeaderMatches:true,deviceExecuted:false},null,2)+'\n');
 return records;
};
