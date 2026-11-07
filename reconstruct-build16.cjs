// SPDX-License-Identifier: MIT
// This local template is copied into the export as reconstruct-build16.cjs.
const fs=require('fs'),p=require('path'),cp=require('child_process');
if(process.argv.length!==5)throw Error('Usage: node reconstruct-build16.cjs <pinned-archive-directory> <absent-output-outside-export> <pinned-glslang>');
const own=__dirname,archives=p.resolve(process.argv[2]),root=p.resolve(process.argv[3]),compiler=p.resolve(process.argv[4]),inside=p.relative(own,root);
if(!inside||(!inside.startsWith('..'+p.sep)&&inside!=='..'&&!p.isAbsolute(inside)))throw Error('Output must stay outside export');
if(fs.existsSync(root))throw Error('Fresh absent output required');
const verify=cp.spawnSync(process.execPath,[p.join(own,'verify-export.cjs')],{encoding:'utf8',windowsHide:true});
if(verify.status!==0)throw Error('Export integrity failed before reconstruction\n'+verify.stderr);
const stages=[];
for(const [script,args]of [['prepare-source.cjs',[archives,root]],['apply-optional-choice.cjs',[root]],['build14-upgrade/apply-build14.cjs',[root]],['apply-build15.cjs',[root]],['apply-build16.cjs',[root]],['generate-all-shaders.cjs',[root,compiler,'--choice']],['verify-source.cjs',[root]]]){
 const q=cp.spawnSync(process.execPath,[p.join(own,script),...args],{encoding:'utf8',windowsHide:true,maxBuffer:32*1024*1024});
 stages.push({script,exit:q.status,stdout:q.stdout||'',stderr:q.stderr||''});
 if(q.status!==0){fs.mkdirSync(root,{recursive:true});fs.writeFileSync(p.join(root,'BUILD16-RECONSTRUCTION.json'),JSON.stringify({passed:false,stages,noSdkBuild:true,noRuntime:true},null,2)+'\n');throw Error('Reconstruction stage failed '+script+'\n'+q.stderr);}
}
fs.writeFileSync(p.join(root,'BUILD16-RECONSTRUCTION.json'),JSON.stringify({passed:true,freshOutputWasAbsent:true,stages,selectedShaderHeaders:8,generatedSpirvModules:13,noOwnerInputs:true,noSdkBuild:true,noRuntime:true},null,2)+'\n');
console.log('Fresh Build16 source reconstruction PASS; no SDK build or target execution');
