// Renders the home-screen icon as a PNG without an image library: a lightning bolt in the design's
// green on the dark panel colour. Full-bleed background with the glyph inside the central 60%, so
// the same file works as a "maskable" icon (Android crops to a circle or squircle) and as "any".
import { deflateSync } from "node:zlib";

const BG = [0x12, 0x16, 0x0e];
const FG = [0xa3, 0xdc, 0x78];
// Bolt outline in a 0..1 square, clockwise.
const BOLT = [[0.58, 0.18], [0.30, 0.55], [0.48, 0.55], [0.40, 0.82], [0.70, 0.43], [0.52, 0.43], [0.62, 0.18]];

function inside(x, y, poly) {
  let hit = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, yi] = poly[i], [xj, yj] = poly[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) hit = !hit;
  }
  return hit;
}

const CRC = new Int32Array(256).map((_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c;
});
function crc32(buf) {
  let c = -1;
  for (const b of buf) c = CRC[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ -1) >>> 0;
}

function chunk(type, data) {
  const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0);
  out.write(type, 4, "ascii");
  data.copy(out, 8);
  out.writeUInt32BE(crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
  return out;
}

export function iconPng(size) {
  const SS = 4;   // 4x4 supersampling per pixel for smooth bolt edges
  const raw = Buffer.alloc(size * (size * 3 + 1));
  for (let py = 0; py < size; py++) {
    const row = py * (size * 3 + 1);
    raw[row] = 0;   // filter: none
    for (let px = 0; px < size; px++) {
      let cover = 0;
      for (let sy = 0; sy < SS; sy++)
        for (let sx = 0; sx < SS; sx++)
          if (inside((px + (sx + 0.5) / SS) / size, (py + (sy + 0.5) / SS) / size, BOLT)) cover++;
      const a = cover / (SS * SS);
      for (let c = 0; c < 3; c++) raw[row + 1 + px * 3 + c] = Math.round(BG[c] + (FG[c] - BG[c]) * a);
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8;   // bit depth
  ihdr[9] = 2;   // colour type: RGB
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", ihdr),
    chunk("IDAT", deflateSync(raw, { level: 9 })),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}
