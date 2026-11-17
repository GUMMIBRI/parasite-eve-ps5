// SPDX-License-Identifier: MIT
// This local template is copied into the new export as apply-build16.cjs.
const fs=require('fs'),p=require('path'),c=require('crypto');
if(process.argv.length!==3)throw Error('Usage: node apply-build16.cjs <prepared-Build15-root>');
const root=p.resolve(process.argv[2]),own=__dirname,m=require('./build16-upgrade/manifest.json'),sha=b=>c.createHash('sha256').update(b).digest('hex');
const safe=s=>typeof s==='string'&&s.length&&!p.isAbsolute(s)&&!s.split(/[\\/]/).some(x=>!x||x==='.'||x==='..');
if(!fs.existsSync(p.join(root,'SOURCE-EXPORT.json')))throw Error('Prepared source marker required');
for(const x of m.files){
 if(!safe(x.path)||!safe(x.file))throw Error('Unsafe upgrade path');
 const f=p.join(root,x.path),post=p.join(own,'build16-upgrade',x.file);
 if(x.before===null){if(fs.existsSync(f))throw Error('Added source already exists '+x.path);}
 else {const s=fs.readFileSync(f,'utf8').replaceAll('\r\n','\n').replaceAll(root.replaceAll('\\','/'),'@ROOT@');if(sha(s)!==x.before)throw Error('Build16 preimage mismatch '+x.path);}
 if(sha(fs.readFileSync(post))!==x.after)throw Error('Build16 postimage mismatch '+x.path);
}
if(fs.existsSync(p.join(root,m.generatedHeader.path)))throw Error('Unexpected pre-existing generated filter header');
for(const x of m.files){fs.mkdirSync(p.dirname(p.join(root,x.path)),{recursive:true});fs.copyFileSync(p.join(own,'build16-upgrade',x.file),p.join(root,x.path));}
console.log('Build16 exact source upgrade applied; filter shader header pending regeneration; caller inputs remain omitted');
