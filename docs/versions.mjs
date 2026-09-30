// Rewrites the manual's edition index on the gh-pages branch (called by .github/publish-pages.sh):
// docs/versions.json for the version picker (assets/docs.js) and docs/index.html, which sends a
// visitor to the newest release's manual (editions are X.Y, one per major/minor release).
import { readdirSync, writeFileSync, statSync } from "node:fs";
import { join } from "node:path";

const dir = process.argv[2];
if (!dir) { console.error("usage: node versions.mjs <gh-pages docs dir>"); process.exit(2); }

const semver = (v) => v.split(/[.-]/).map((p) => (/^\d+$/.test(p) ? Number(p) : p));
const newer = (a, b) => {
  const x = semver(a), y = semver(b);
  for (let i = 0; i < Math.max(x.length, y.length); i++) if (x[i] !== y[i]) return (x[i] ?? -1) > (y[i] ?? -1) ? -1 : 1;
  return 0;
};

const editions = readdirSync(dir).filter((d) => statSync(join(dir, d)).isDirectory());
const releases = editions.filter((d) => /^\d+\.\d+(\.\d+)?$/.test(d)).sort(newer);
const list = [
  ...(editions.includes("latest") && releases.length ? [{ id: "latest", title: `${releases[0]} (latest)` }] : []),
  ...releases.map((v) => ({ id: v, title: v })),
];
writeFileSync(join(dir, "versions.json"), JSON.stringify(list, null, 2) + "\n");

const target = editions.includes("latest") ? "latest" : releases[0];
writeFileSync(join(dir, "index.html"), `<!doctype html>
<html><head><meta charset="utf-8"><title>METER</title>
<meta http-equiv="refresh" content="0; url=${target}/">
<link rel="canonical" href="${target}/"></head>
<body><a href="${target}/">METER manual</a></body></html>
`);
console.log(`versions           ${list.map((e) => e.id).join(", ")} -> index ${target}/`);
