// Bundles src/ into dist/index.html (single file, JS+CSS inlined) and dist/index.html.gz
// for embedding into the ESPHome firmware.
import { build } from "esbuild";
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { gzipSync } from "node:zlib";

const watch = process.argv.includes("--watch");

const result = await build({
  entryPoints: ["src/main.js"],
  bundle: true,
  minify: true,
  format: "iife",
  target: ["es2018"],
  write: false,
  define: { "process.env.NODE_ENV": '"production"' },
  legalComments: "none",
});
const js = result.outputFiles[0].text;
const css = readFileSync("src/style.css", "utf8").replace(/\s+/g, " ").trim();
const html = readFileSync("src/index.html", "utf8")
  .replace("/*CSS*/", () => css)
  .replace("/*JS*/", () => js.replace(/<\/script/gi, "<\\/script"));

mkdirSync("dist", { recursive: true });
writeFileSync("dist/index.html", html);
const gz = gzipSync(Buffer.from(html), { level: 9 });
writeFileSync("dist/index.html.gz", gz);
console.log(`dist/index.html     ${(html.length / 1024).toFixed(1)} kB`);
console.log(`dist/index.html.gz  ${(gz.length / 1024).toFixed(1)} kB`);
