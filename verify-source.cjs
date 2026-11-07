// SPDX-License-Identifier: MIT
// Caller-free final source verification, before SDK work or owner generation.
const fs=require('fs'),p=require('path'),c=require('crypto');
if(process.argv.length!==3)throw Error('Usage: node verify-source.cjs <prepared-Build21-root>');
const root=p.resolve(process.argv[2]),sha=s=>c.createHash('sha256').update(s).digest('hex'),m=require('./SELECTED-SOURCE.json');
for(const x of m.files){const s=fs.readFileSync(p.join(root,x.path),'utf8').replaceAll('\r\n','\n').replaceAll(root.replaceAll('\\','/'),'@ROOT@');if(sha(s)!==x.sha256)throw Error('Selected source mismatch '+x.path);}
for(const x of require('./build15-upgrade/manifest.json').removed)if(fs.existsSync(p.join(root,x.path)))throw Error('Removed installer present '+x.path);
for(const file of ['game-src/include/pe1/point_sprite_owner.inc','POINTSPRITE-OWNER-INPUT.json','STATE-SOURCE-IDENTITY.json','INPUT-PREFLIGHT.json','game-src/src/main/boot/state_build_identity.h'])if(fs.existsSync(p.join(root,file)))throw Error('Caller-free verification requires omitted generated input '+file);
const index=fs.readFileSync(p.join(root,'game-src/src/main/boot/installer/pe_native_pack_index.inc'),'utf8');if(!index.includes('PE_NATIVE_PACK_INDEX_AVAILABLE 0')||!index.includes('{NULL,0,{0}}'))throw Error('Expected empty public pack');
console.log('Verified '+m.files.length+' selected Build21 source/mirror/support/shader records; three removals; caller inputs omitted');
