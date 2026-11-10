// app.js - shared helpers, API client and page routing.
// Page scripts register themselves with NFTApp.register(name, { onShow }).
(function () {
  'use strict';

  var TITLES = {
    dashboard: 'Dashboard',
    lookup: 'NFT Ownership Lookup',
    history: 'Ownership History',
    benchmark: 'Performance Comparison',
    hashstats: 'Hash Table Statistics'
  };

  // The C++ server serves this frontend itself (http://localhost:8080/). If the
  // files are opened any other way (file://, VS Code Live Server) we call the
  // server on port 8080 directly; CORS is enabled on the backend.
  // Override with  index.html?api=http://host:port
  function resolveApiBase() {
    var q = new URLSearchParams(window.location.search).get('api');
    if (q) return q.replace(/\/+$/, '');
    if (window.location.protocol.indexOf('http') === 0 && window.location.port === '8080') return '';
    return 'http://localhost:8080';
  }
  var API_BASE = resolveApiBase();

  var pages = {};
  var current = null;

  function api(path, options) {
    return fetch(API_BASE + path, options).then(function (res) {
      return res.text().then(function (text) {
        var data = null;
        try { data = JSON.parse(text); } catch (e) { /* not JSON */ }
        if (data === null) {
          var err = new Error('Server returned an unexpected response (HTTP ' + res.status + ')');
          err.status = res.status;
          throw err;
        }
        data._status = res.status;
        return data;
      });
    });
  }

  function fmtInt(n) { return Number(n).toLocaleString('en-US'); }
  function fmtNum(n, digits) {
    return Number(n).toLocaleString('en-US', { minimumFractionDigits: digits, maximumFractionDigits: digits });
  }

  function el(id) { return document.getElementById(id); }

  function showMessage(id, kind, text) {
    var box = el(id);
    box.className = 'message ' + kind;
    box.textContent = text;
  }
  function hideMessage(id) { el(id).className = 'message hidden'; }

  // Token ID validation mirrors the server: positive integer, up to 18 digits.
  function validTokenId(text) { return /^[0-9]{1,18}$/.test(text) && Number(text) > 0; }

  function showBanner(text) {
    var b = el('banner');
    if (text) { b.textContent = text; b.classList.remove('hidden'); } else { b.classList.add('hidden'); }
  }

  function checkServer() {
    return api('/api/health').then(function (d) {
      el('serverStatus').className = 'server-status ok';
      el('serverStatusText').textContent = 'Backend online (' + fmtInt(d.nfts) + ' NFTs)';
      showBanner('');
      return true;
    }).catch(function () {
      el('serverStatus').className = 'server-status bad';
      el('serverStatusText').textContent = 'Backend offline';
      showBanner('Cannot reach the C++ backend at ' + (API_BASE || window.location.origin) + '. Start nft_server first (see README), then reload this page.');
      return false;
    });
  }

  function show(name) {
    if (!pages[name]) name = 'dashboard';
    current = name;
    Object.keys(pages).forEach(function (p) {
      el('page-' + p).classList.toggle('hidden', p !== name);
    });
    document.querySelectorAll('.nav-link').forEach(function (a) {
      a.classList.toggle('active', a.getAttribute('data-page') === name);
    });
    el('pageTitle').textContent = TITLES[name];
    el('sidebar').classList.remove('open');
    if (pages[name].onShow) pages[name].onShow();
  }

  function route() {
    show((window.location.hash || '#dashboard').replace('#', ''));
  }

  // Jump to a page and optionally prefill a token ID input and submit it.
  function goTo(name, inputId, value, formId) {
    window.location.hash = '#' + name;
    if (inputId) {
      el(inputId).value = value;
      if (formId) el(formId).dispatchEvent(new Event('submit', { cancelable: true }));
    }
  }

  window.NFTApp = {
    api: api, el: el, fmtInt: fmtInt, fmtNum: fmtNum,
    showMessage: showMessage, hideMessage: hideMessage, validTokenId: validTokenId,
    goTo: goTo, checkServer: checkServer,
    register: function (name, page) { pages[name] = page; },
    start: function () {
      el('menuBtn').addEventListener('click', function () { el('sidebar').classList.toggle('open'); });
      window.addEventListener('hashchange', route);
      checkServer().then(route);
      window.addEventListener('resize', function () {
        if (current && pages[current] && pages[current].onResize) pages[current].onResize();
      });
    }
  };
})();
