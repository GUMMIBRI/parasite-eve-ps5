// SPDX-License-Identifier: MIT
// This local template is copied into the new export as apply-build21.cjs.
const fs=require('fs'),p=require('path'),c=require('crypto');
if(process.argv.length!==3)throw Error('Usage: node apply-build21.cjs <prepared-Build20-root>');
const root=p.resolve(process.argv[2]),own=__dirname,m=require('./build21-upgrade/manifest.json'),sha=b=>c.createHash('sha256').update(b).digest('hex');
const safe=s=>typeof s==='string'&&s.length&&!p.isAbsolute(s)&&!s.split(/[\\/]/).some(x=>!x||x==='.'||x==='..');
if(!fs.existsSync(p.join(root,'SOURCE-EXPORT.json')))throw Error('Prepared source marker required');
for(const x of m.files){
 if(!safe(x.path)||!safe(x.file))throw Error('Unsafe upgrade path');
 const f=p.join(root,x.path),post=p.join(own,'build21-upgrade',x.file);
 if(x.before===null){if(fs.existsSync(f))throw Error('Added source already exists '+x.path);}
 else {const s=fs.readFileSync(f,'utf8').replaceAll('\r\n','\n').replaceAll(root.replaceAll('\\','/'),'@ROOT@');if(sha(s)!==x.before)throw Error('Build21 preimage mismatch '+x.path);}
 if(sha(fs.readFileSync(post))!==x.after)throw Error('Build21 postimage mismatch '+x.path);
}

for(const x of m.files){fs.mkdirSync(p.dirname(p.join(root,x.path)),{recursive:true});fs.copyFileSync(p.join(own,'build21-upgrade',x.file),p.join(root,x.path));}
console.log('Build21 exact source upgrade applied; final app-priority/branding/selected label-source upgrade applied; caller inputs remain omitted');
