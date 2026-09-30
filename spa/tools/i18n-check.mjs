// Compares language tables against the first one (the reference, de). Shared by build.mjs and the
// user manual build (docs/build.mjs), which resolves UI labels from the same tables and must refuse
// the same mismatches. Returns the problems instead of exiting so each caller reports them its way.
export function checkLanguageTables(tables) {
  const [refName, ref] = Object.entries(tables)[0];
  const problems = [];
  for (const [name, t] of Object.entries(tables).slice(1)) {
    for (const k of Object.keys(ref)) {
      if (!(k in t)) problems.push(`${name}: missing key "${k}"`);
      else if (typeof t[k] !== typeof ref[k]) problems.push(`${name}: "${k}" is ${typeof t[k]}, ${refName} has ${typeof ref[k]}`);
      else if (typeof t[k] === "function" && t[k].length !== ref[k].length) problems.push(`${name}: "${k}" takes ${t[k].length} arguments, ${refName} passes ${ref[k].length}`);
    }
    for (const k of Object.keys(t)) if (!(k in ref)) problems.push(`${name}: unknown key "${k}" (not in ${refName})`);
  }
  return problems;
}
