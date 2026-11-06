// SPDX-License-Identifier: MIT
const fs=require('fs'),p=require('path'),c=require('crypto'),sha=b=>c.createHash('sha256').update(b).digest('hex');
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(p.join(d,e.name)):[p.join(d,e.name)]);
const rows=require('./EXPORT-CHECKSUMS.json'),actual=walk(__dirname).map(f=>p.relative(__dirname,f).replaceAll('\\','/')).filter(f=>f!=='EXPORT-CHECKSUMS.json').sort();
if(JSON.stringify(actual)!==JSON.stringify(rows.map(x=>x.path).sort()))throw Error('Export inventory mismatch');
for(const x of rows){const b=fs.readFileSync(p.join(__dirname,x.path));if(b.length!==x.bytes||sha(b)!==x.sha256)throw Error('Export checksum mismatch '+x.path);}console.log('Export inventory and '+rows.length+' file checksums verified');
