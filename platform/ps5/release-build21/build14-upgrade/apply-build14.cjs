// SPDX-License-Identifier: MIT
const fs=require('fs'),p=require('path'),c=require('crypto'),m=require('./manifest.json'),root=p.resolve(process.argv[2]||'');
if(process.argv.length!==3)throw Error('Usage: node apply-build14.cjs <prepared-Build13-with-choice>');
const sha=b=>c.createHash('sha256').update(b).digest('hex'),norm=b=>b.toString().replaceAll('\r\n','\n').replaceAll(root.replaceAll('\\','/'),'@ROOT@');
for(const x of m.files){if(!/^(game-src|psyz-src|game-support|state)\//.test(x.path)||x.path.split('/').some(v=>!v||v==='..'||v==='.'))throw Error('Unsafe path');const f=p.join(root,x.path),b=fs.existsSync(f)?norm(fs.readFileSync(f)):null;if(x.before===null?b!==null:b===null||sha(b)!==x.before)throw Error('Preimage '+x.path);if(sha(fs.readFileSync(p.join(__dirname,'overlay',x.file)))!==x.after)throw Error('Postimage '+x.path);}
for(const x of m.files){const f=p.join(root,x.path);fs.mkdirSync(p.dirname(f),{recursive:true});fs.writeFileSync(f,fs.readFileSync(p.join(__dirname,'overlay',x.file),'utf8'));}
console.log('Build14 source overlay applied; generated owner inputs remain external.');
