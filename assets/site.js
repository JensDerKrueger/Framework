(function () {
  function ready(fn) {
    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", fn);
    } else {
      fn();
    }
  }

  function text(value) {
    var span = document.createElement("span");
    span.innerHTML = value || "";
    return span.textContent || span.innerText || "";
  }

  function monthName(month) {
    var names = ["", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    return names[Number(month)] || month || "";
  }

  function firstLink(links) {
    if (!links || !links.length) return null;
    return links[0].url || null;
  }

  function publicationLink(paper) {
    if (paper.pdf) return paper.pdf;
    if (paper.doi) return "https://doi.org/" + paper.doi;
    if (paper.bib) return paper.bib;
    return firstLink(paper.links);
  }

  function createItem(meta, title, body, href) {
    var article = document.createElement("article");
    article.className = "list-item";

    var metaEl = document.createElement("p");
    metaEl.className = "item-meta";
    metaEl.textContent = meta;

    var titleEl = document.createElement("h3");
    if (href) {
      var link = document.createElement("a");
      link.href = href;
      link.textContent = title;
      titleEl.appendChild(link);
    } else {
      titleEl.textContent = title;
    }

    var bodyEl = document.createElement("p");
    bodyEl.textContent = body;

    article.appendChild(metaEl);
    article.appendChild(titleEl);
    article.appendChild(bodyEl);
    return article;
  }

  function latestNews(limit) {
    if (!window.newsData || !newsData.years) return [];
    var items = [];
    newsData.years.forEach(function (year) {
      (year.news || []).forEach(function (entry) {
        items.push({
          year: year.value,
          month: entry.month,
          title: text(entry.title),
          body: text(entry.text).replace(/\s+/g, " ").trim(),
          href: firstLink(entry.links)
        });
      });
    });
    return items.slice(0, limit);
  }

  function latestPublications(limit) {
    if (!window.publicationData || !publicationData.years) return [];
    var items = [];
    publicationData.years.forEach(function (year) {
      (year.papers || []).forEach(function (paper) {
        items.push({
          year: year.value,
          month: paper.month,
          title: text(paper.title),
          body: text(paper.event),
          href: publicationLink(paper)
        });
      });
    });
    return items.slice(0, limit);
  }

  function renderList(id, items, emptyText) {
    var target = document.getElementById(id);
    if (!target) return;
    target.innerHTML = "";
    if (!items.length) {
      var empty = document.createElement("p");
      empty.className = "muted";
      empty.textContent = emptyText;
      target.appendChild(empty);
      return;
    }
    items.forEach(function (item) {
      var meta = [monthName(item.month), item.year].filter(Boolean).join(" ");
      target.appendChild(createItem(meta, item.title, item.body, item.href));
    });
  }

  function initNav() {
    var button = document.querySelector(".nav-toggle");
    var nav = document.getElementById("site-nav");
    if (!button || !nav) return;

    button.addEventListener("click", function () {
      var expanded = button.getAttribute("aria-expanded") === "true";
      button.setAttribute("aria-expanded", String(!expanded));
      nav.classList.toggle("open", !expanded);
    });
  }

  ready(function () {
    initNav();
    renderList("latest-news", latestNews(3), "No news entries available.");
    renderList("latest-publications", latestPublications(3), "No publications available.");
  });
}());
