// history.js - Page 3: ownership timeline and transfer form
(function () {
  'use strict';
  var A = window.NFTApp;
  var currentToken = null;

  function span(cls, text) {
    var s = document.createElement('span');
    if (cls) s.className = cls;
    s.textContent = text;
    return s;
  }

  function render(d) {
    currentToken = d.tokenId;
    A.el('hist-token').textContent = d.tokenId;
    A.el('hist-owner').textContent = d.currentOwner;
    A.el('hist-count').textContent = A.fmtInt(d.count);

    var list = A.el('timeline');
    list.textContent = '';
    d.history.forEach(function (t, i) {
      var li = document.createElement('li');
      var isMint = t.previousOwner === 'MINT';
      var isLast = i === d.history.length - 1;
      li.className = 'tl-item' + (isMint ? ' mint' : '') + (isLast ? ' latest' : '');

      var box = document.createElement('div');
      box.className = 'tl-box';

      var head = document.createElement('div');
      head.className = 'tl-head';
      head.appendChild(span('', '#' + (i + 1) + (isMint ? ' · mint' : ' · transfer')));
      head.appendChild(span('', t.time));
      box.appendChild(head);

      var flow = document.createElement('div');
      flow.className = 'tl-flow';
      flow.appendChild(span('', t.previousOwner));
      flow.appendChild(span('tl-arrow', '→'));
      flow.appendChild(span('', t.newOwner));
      if (isLast) flow.appendChild(span('tag cur', 'current owner'));
      box.appendChild(flow);

      li.appendChild(box);
      list.appendChild(li);
    });
    A.el('historyResult').classList.remove('hidden');
  }

  function load(text) {
    return A.api('/api/nfts/' + text + '/history').then(function (d) {
      if (!d.success) {
        A.el('historyResult').classList.add('hidden');
        A.showMessage('historyMessage', d._status === 404 ? 'info' : 'error',
          d._status === 404 ? 'No NFT with Token ID ' + text + ' exists in this dataset.' : (d.error || 'Request failed'));
        return;
      }
      A.hideMessage('historyMessage');
      render(d);
    });
  }

  function init() {
    A.el('historyForm').addEventListener('submit', function (ev) {
      ev.preventDefault();
      var text = A.el('historyId').value.trim();
      A.hideMessage('transferMessage');
      if (!A.validTokenId(text)) {
        A.showMessage('historyMessage', 'error', 'Enter a Token ID: a positive whole number.');
        return;
      }
      load(text).catch(function (e) { A.showMessage('historyMessage', 'error', 'Request failed: ' + e.message); });
    });

    A.el('transferForm').addEventListener('submit', function (ev) {
      ev.preventDefault();
      var owner = A.el('newOwner').value.trim();
      if (currentToken === null) return;
      if (!owner) { A.showMessage('transferMessage', 'error', 'Enter the new owner.'); return; }
      // text/plain keeps this a "simple" CORS request (no preflight) - the server parses the JSON body regardless.
      A.api('/api/nfts/' + currentToken + '/transfer', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: JSON.stringify({ newOwner: owner })
      }).then(function (d) {
        if (!d.success) { A.showMessage('transferMessage', 'error', d.error || 'Transfer failed'); return; }
        A.showMessage('transferMessage', 'ok', 'Transfer recorded' + (d.persisted ? ' and saved to CSV.' : ' (CSV write failed; kept in memory).'));
        A.el('newOwner').value = '';
        return load(String(currentToken));
      }).catch(function (e) { A.showMessage('transferMessage', 'error', 'Request failed: ' + e.message); });
    });
  }

  A.register('history', { onShow: function () {} });
  init();
})();
