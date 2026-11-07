// SPDX-License-Identifier: MIT
const fs=require('fs'),p=require('path'),c=require('crypto');
module.exports=function(root,base){const norm=s=>s.replaceAll('\\','/'),sha=s=>c.createHash('sha256').update(s).digest('hex'),rows=require('./SELECTED-SOURCE.json').files.filter(x=>x.path.startsWith('game-support/'));
const pending=rows.map(x=>{const file=p.join(root,x.path),s=fs.readFileSync(file,'utf8'),v=s.replaceAll('\r\n','\n').replaceAll(norm(root),'@ROOT@').replaceAll(norm(base),'@BASE@');if(sha(v)!==x.sha256)throw Error('Selected support mismatch '+x.path);return {file,s};});
for(const {file,s}of pending)fs.writeFileSync(file,s.replaceAll('@ROOT@',norm(root)).replaceAll('@BASE@',norm(base)));return pending.length;};
