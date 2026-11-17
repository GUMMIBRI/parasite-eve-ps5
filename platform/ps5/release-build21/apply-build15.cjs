// SPDX-License-Identifier: MIT
const fs=require('fs'),p=require('path'),c=require('crypto'),m=require('./build15-upgrade/manifest.json'),root=p.resolve(process.argv[2]||''),sha=b=>c.createHash('sha256').update(b).digest('hex');
if(process.argv.length!==3)throw Error('Usage: node apply-build15.cjs <prepared-public14>');
for(const x of m.files){const f=p.join(root,x.path),s=fs.readFileSync(f,'utf8').replaceAll('\r\n','\n').replaceAll(root.replaceAll('\\','/'),'@ROOT@');if(sha(s)!==x.before||sha(fs.readFileSync(p.join(__dirname,'build15-upgrade',x.file)))!==x.after)throw Error('Pre/postimage '+x.path);}
for(const x of m.removed||[]){const f=p.join(root,x.path);if(sha(fs.readFileSync(f))!==x.before)throw Error('Removed preimage '+x.path);}
for(const x of m.removed||[])fs.unlinkSync(p.join(root,x.path));
for(const x of m.files)fs.copyFileSync(p.join(__dirname,'build15-upgrade',x.file),p.join(root,x.path));
console.log('Build15 exact source upgrade applied; caller input remains omitted');
