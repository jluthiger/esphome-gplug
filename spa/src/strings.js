// The app imports its text from here. The tables themselves, the language switch and the
// fallback live in i18n/; this module stays as the single import surface the rest of the SPA
// sees, so adding a language never touches a component.
export { S, LANGS, presetLabel, getLang, setLang, getLocale, onLangChange, applyLang } from "./i18n/index.js";
