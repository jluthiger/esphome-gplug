// Where the wide-screen layout starts. One constant for both halves of it: build.mjs writes it into
// the media attribute of the style.wide.css block, and useWide() (wide.js) matches it for the
// components that render a different structure there. Kept free of imports so build.mjs, which
// runs in Node, can read it without pulling in Preact.
//
// Below it nothing changes: phones get style.css, and 800-1023 px (a tablet held upright) keeps
// the stretched phone layout of style.desktop.css, which suits a column that narrow.
export const WIDE = "(min-width:1024px)";
