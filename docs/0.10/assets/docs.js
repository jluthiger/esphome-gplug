// Keeps the reader's place when switching language (same page, same story anchor) and offers the
// other editions when the site lists them. Editions live side by side on GitHub Pages
// (docs/0.8/, docs/0.9/, docs/latest/, ...) with docs/versions.json next to them; a local build
// has no such file, so the version picker stays hidden.
(function () {
  var page = document.documentElement.dataset.page;
  var lang = document.documentElement.lang;
  document.querySelectorAll("a.lang").forEach(function (a) {
    a.addEventListener("click", function () { a.href = a.getAttribute("href") + location.hash; });
  });

  var label = document.querySelector(".ver");
  var parts = location.pathname.split("/");   // .../docs/<edition>/<lang>/<page>
  var edition = parts[parts.length - 3];
  fetch("../../versions.json").then(function (r) { return r.ok ? r.json() : null; }).then(function (list) {
    if (!list || !list.length) return;
    var sel = label.querySelector("select");
    list.forEach(function (v) {
      var o = document.createElement("option");
      o.value = v.id; o.textContent = v.title || v.id; o.selected = v.id === edition;
      sel.appendChild(o);
    });
    sel.addEventListener("change", function () {
      location.href = "../../" + sel.value + "/" + lang + "/" + page + location.hash;
    });
    label.hidden = false;
  }).catch(function () {});
})();
