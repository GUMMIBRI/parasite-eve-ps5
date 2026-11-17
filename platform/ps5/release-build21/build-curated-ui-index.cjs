'use strict';
// MIT. Bounded metadata-only index extension; no pixel decode, scan or asset copy.
// Output is private derived pack metadata. It is not a classification oracle.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
function need(v,m){if(!v)throw Error(m);}
function workspace(file){need(process.env.PE_PRIVATE_INPUT_ROOT,'set caller-owned PE_PRIVATE_INPUT_ROOT outside the export');const bound=path.resolve(process.env.PE_PRIVATE_INPUT_ROOT),inside=path.relative(__dirname,bound);need(inside==='..'||inside.startsWith('..'+path.sep)||path.isAbsolute(inside),'private root must be outside export');const resolved=path.resolve(file),relative=path.relative(bound,resolved);need(relative&&!relative.startsWith('..')&&!path.isAbsolute(relative),'all files must stay under caller private root');let part=path.dirname(resolved);for(;;){need(!fs.lstatSync(part).isSymbolicLink(),'linked parent excluded');const parent=path.dirname(part);if(parent===part)break;part=parent;}return resolved;}
function read(file,limit){file=workspace(file);const s=fs.lstatSync(file);need(s.isFile()&&!s.isSymbolicLink()&&s.size<=limit,'unsafe input type/size');return fs.readFileSync(file);}
function hex(v){need(typeof v==='string'&&/^[a-fA-F0-9]{16}$/.test(v),'invalid source identity');return BigInt('0x'+v);}
function main(manifestPath,basePath,output){
  output=workspace(output);
  const manifest=JSON.parse(read(manifestPath,1024*1024));
  need(manifest.schema==='pe.duck-ui-selection.v1'&&Array.isArray(manifest.entries)&&manifest.entries.length>0&&manifest.entries.length<=512,'explicit nonempty UI selection required (maximum512)');
  need(typeof manifest.metadataSha256==='string'&&/^[a-f0-9]{64}$/.test(manifest.metadataSha256),'pinned metadata SHA256 required');
  const metadataBytes=read(manifest.metadata,256*1024*1024);need(sha(metadataBytes)===manifest.metadataSha256,'metadata snapshot changed');
  const metadata=JSON.parse(metadataBytes);need(metadata.schema==='cpe.duck-pack-index.v1'&&Array.isArray(metadata.entries),'unsupported metadata schema');
  const base=read(basePath,16+45000*282);need(base.length>=16&&base.subarray(0,8).toString()==='PEDUCK01'&&base.readUInt16LE(12)===282&&!base[14]&&!base[15],'unsupported base index');
  need(typeof manifest.baseModelIndexSha256==='string'&&/^[a-f0-9]{64}$/.test(manifest.baseModelIndexSha256)&&sha(base)===manifest.baseModelIndexSha256,'pinned model base changed');
  const count=base.readUInt32LE(8);need(count>0&&count<=45000&&base.length===16+count*282,'base length/count mismatch');
  const records=[],plan=[],seen=new Set();
  for(let i=0;i<count;i++){
    const record=base.subarray(16+i*282,16+(i+1)*282);const zero=record.indexOf(0,26);need(zero>26,'invalid base filename');
    const name=record.subarray(26,zero).toString();need(!name.startsWith('ui-'),'base must be unchanged model-only index');
    seen.add(name.toLowerCase());
  }
  for(const selection of manifest.entries){
    need(Number.isInteger(selection.metadataEntry)&&selection.metadataEntry>=0&&selection.metadataEntry<metadata.entries.length,'metadata entry out of range');
    need(['menu','text','character-select','cursor'].includes(selection.category),'only positively classified non-environment UI accepted');
    need(typeof selection.classificationEvidence==='string'&&selection.classificationEvidence.trim().length>=16,'record source/UV or reviewed visual classification evidence');
    const e=metadata.entries[selection.metadataEntry],id=e.identity,png=e.png;
    need(e.configConsumerReady===true,'metadata config not supported');
    need(e.classification!=='environment'&&e.classification!=='background'&&id&&id.kind==='texpage'&&['P4','P8'].includes(id.mode),'unsupported or environment identity');
    const mode=id.mode==='P8'?1:0,factor=mode?2:4;
    need(id.sourceWords?.[0]===(mode?128:64)&&id.sourceWords?.[1]===256,'only full registered texture-page source dimensions supported');
    const [x,y,w,h]=id.rectangleTexels||[],[lo,hi]=id.paletteRangeTokens||[];
    need([x,y,w,h,lo,hi].every(Number.isInteger)&&lo===0&&hi>=0&&hi<(mode?256:16)&&x>=0&&y>=0&&w>0&&h>0&&x+w<=256&&y+h<=256&&x%factor===0&&w%factor===0,'unsupported rectangle/palette coverage');
    const ihdr=png?.ihdr;need(ihdr&&ihdr.width>=w&&ihdr.height>=h&&ihdr.width<=2048&&ihdr.height<=1024&&ihdr.width%w===0&&ihdr.height%h===0&&ihdr.width/w===ihdr.height/h&&ihdr.width/w<=8,'replacement dimensions outside native budget');
    need(typeof png.sha256==='string'&&/^[a-f0-9]{64}$/i.test(png.sha256),'PNG provenance required');
    const original=e.relative;need(typeof original==='string'&&/^[A-Za-z0-9_.-]+\.png$/.test(original)&&!original.includes('..')&&original===id.rawIdentity+'.png','safe source basename required');
    const aliases=metadata.byIdentity?.[id.key];need(Array.isArray(aliases)&&aliases.length>0&&aliases.length<=1024,'identity alias proof missing or excessive');
    for(const alias of aliases)need(Number.isInteger(alias)&&metadata.entries[alias]?.png?.sha256===png.sha256,'unresolved source identity collision');
    const name='ui-'+selection.category+'-'+original;need(Buffer.byteLength(name)<256&&!seen.has(name.toLowerCase()),'duplicate or oversized output filename');seen.add(name.toLowerCase());
    const record=Buffer.alloc(282);record[0]=mode;record[1]=hi;[x,y,w,h].forEach((v,i)=>record.writeUInt16LE(v,2+i*2));record.writeBigUInt64LE(hex(id.textureHash),10);record.writeBigUInt64LE(hex(id.paletteHash),18);record.write(name,26);
    // Reject unresolved identity collisions, including different metadata PNGs.
    for(const prior of plan)need(prior.identity!==id.key,'select one alias for each source identity');
    records.push(record);plan.push({metadataEntry:selection.metadataEntry,category:selection.category,classificationEvidence:selection.classificationEvidence,identity:id.key,source:e.source,destination:name,pngSha256:png.sha256,bytes:png.bytes});
  }
  need(count+records.length<=45000,'combined index count exceeds native bound');
  const header=Buffer.from(base.subarray(0,16));header.writeUInt32LE(count+records.length,8);const result=Buffer.concat([header,base.subarray(16),...records]);
  need(!fs.existsSync(output)&&!fs.existsSync(output+'.copy-plan.json'),'exclusive output required');
  fs.writeFileSync(output,result,{flag:'wx'});
  fs.writeFileSync(output+'.copy-plan.json',JSON.stringify({private:true,schema:'pe.duck-ui-copy-plan.v1',baseSha256:sha(base),metadataSha256:sha(metadataBytes),indexSha256:sha(result),modelRecordsPreserved:count,uiRecords:records.length,imagesCopied:false,entries:plan},null,2)+'\n',{flag:'wx'});
  console.log(JSON.stringify({modelRecordsPreserved:count,uiRecords:records.length,indexSha256:sha(result),imagesCopied:false}));
}
if(require.main===module){try{need(process.argv.length===5,'usage: node build-curated-ui-index.cjs selection.json base-model-index.bin output.bin');main(...process.argv.slice(2));}catch(e){console.error(e.message);process.exitCode=1;}}
module.exports={main};
