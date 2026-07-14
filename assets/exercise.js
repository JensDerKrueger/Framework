(() => {
  const loading = document.querySelector(".canvas-container .loading-text#loading");

  if (!loading)
    return;

  const hideLoading = () => {
    loading.textContent = "";
    loading.style.display = "none";
    loading.setAttribute("aria-hidden", "true");
  };

  const showLoading = text => {
    if (!text) {
      hideLoading();
      return;
    }

    loading.textContent = text;
    loading.style.display = "block";
    loading.removeAttribute("aria-hidden");
  };

  const moduleConfig = window.Module || {};
  const previousSetStatus = moduleConfig.setStatus;
  const previousRuntimeInitialized = moduleConfig.onRuntimeInitialized;

  moduleConfig.setStatus = text => {
    if (typeof previousSetStatus === "function")
      previousSetStatus(text);

    showLoading(text);
  };

  moduleConfig.onRuntimeInitialized = function (...args) {
    if (typeof previousRuntimeInitialized === "function")
      previousRuntimeInitialized.apply(this, args);

    hideLoading();
  };

  window.Module = moduleConfig;

  window.addEventListener("error", event => {
    const target = event.target;
    if (!(target instanceof HTMLScriptElement))
      return;

    const source = target.getAttribute("src") || "";
    if (/(^|\/)[Ss]olution\.js($|\?)/.test(source))
      showLoading("Could not load solution");
  }, true);

  window.setTimeout(() => {
    const text = loading.textContent.trim().toLowerCase();
    if (text === "loading" || text === "lade")
      hideLoading();
  }, 12000);
})();
