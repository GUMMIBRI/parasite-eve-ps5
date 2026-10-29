/* Test-only fixture generator. Node built-ins only; no files read except code.
 * Run: node platform/ps5/hd/tests/generate-lookup-png.cjs
 * Writes only synthetic_lookup_png.h next to this script. No game assets.
 */
const fs = require('node:fs');
const zlib = require('node:zlib');
const path = require('node:path');
function crc(bytes) {
    let c = 0xffffffff;
    for (const v of bytes) {
        c ^= v;
        for (let i = 0; i < 8; i++) c = (c >>> 1) ^ ((c & 1) ? 0xedb88320 : 0);
    }
    return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, bytes) {
    const h = Buffer.alloc(4), c = Buffer.alloc(4), t = Buffer.from(type);
    h.writeUInt32BE(bytes.length);
    c.writeUInt32BE(crc(Buffer.concat([t, bytes])));
    return Buffer.concat([h, t, bytes, c]);
}
let text = '/* Synthetic constant RGBA(12,34,56,78) PNGs, filter 0, zlib level 9. No game assets. */\n';
for (const n of [256, 512]) {
    const raw = Buffer.alloc((1 + n * 4) * n), rgba = Buffer.from([12, 34, 56, 78]);
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) rgba.copy(raw, y * (1 + n * 4) + 1 + x * 4);
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(n); ihdr.writeUInt32BE(n, 4); ihdr[8] = 8; ihdr[9] = 6;
    const png = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
        chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, {level: 9})), chunk('IEND', Buffer.alloc(0))]);
    text += `static const unsigned char lookup_png_${n}[] = {\n`;
    for (let i = 0; i < png.length; i += 24) text += [...png.subarray(i, i + 24)].join(',') + ',\n';
    text += '};\n';
}
fs.writeFileSync(path.join(__dirname, 'synthetic_lookup_png.h'), text);

