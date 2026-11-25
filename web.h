#ifndef WEB_H
#define WEB_H

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Coffee Scale</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:Georgia,'Times New Roman',serif;background:#faf8f5;color:#2c2420;padding:20px 16px;max-width:480px;margin:0 auto}
    .hdr{text-align:center;margin-bottom:22px}
    .hdr h1{font-size:1.5em;font-weight:400;color:#2c2420;letter-spacing:1px}
    .hdr small{font-size:0.65em;color:#b0a898;letter-spacing:2px;text-transform:uppercase;font-family:-apple-system,BlinkMacSystemFont,sans-serif}
    .status{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:20px}
    .status-item{text-align:center;padding:12px 4px;background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(44,36,32,0.06)}
    .status-label{font-size:0.6em;color:#b0a898;text-transform:uppercase;letter-spacing:1px;margin-bottom:4px;font-family:-apple-system,BlinkMacSystemFont,sans-serif}
    .status-value{font-family:'Courier New',monospace;font-size:1.1em;color:#e85d3a;font-weight:700}
    .section{margin-bottom:20px}
    .section-title{font-size:0.62em;color:#b0a898;text-transform:uppercase;letter-spacing:2.5px;margin-bottom:10px;font-family:-apple-system,BlinkMacSystemFont,sans-serif}
    .setting{display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:10px 14px;background:#fff;border-radius:10px;box-shadow:0 1px 4px rgba(44,36,32,0.06);transition:box-shadow .2s}
    .setting:hover{box-shadow:0 2px 8px rgba(44,36,32,0.1)}
    .setting label{flex:1;font-size:0.85em;color:#5c524a;font-family:-apple-system,BlinkMacSystemFont,sans-serif}
    .setting input{width:68px;background:#faf8f5;color:#2c2420;border:1.5px solid #e8e2da;border-radius:8px;padding:8px;text-align:right;font-family:'Courier New',monospace;font-size:0.9em;transition:border-color .2s}
    .setting input:focus{border-color:#e85d3a;outline:none}
    .unit{font-size:0.72em;color:#b0a898;width:10px;font-family:'Courier New',monospace}
    button{background:#e85d3a;color:#fff;border:none;border-radius:8px;padding:8px 14px;font-size:0.72em;cursor:pointer;font-family:-apple-system,BlinkMacSystemFont,sans-serif;font-weight:600;letter-spacing:0.5px;transition:all .15s;text-transform:uppercase}
    button:hover{background:#d4502f}
    button:active{transform:scale(.97);background:#c04528}
    .feedback{text-align:center;padding:6px;font-size:0.78em;color:#2aa694;min-height:22px;font-family:'Courier New',monospace}
    .feedback.show{animation:fin .3s ease}
    @keyframes fin{from{opacity:0;transform:translateY(4px)}to{opacity:1;transform:translateY(0)}}
    canvas{width:100%;background:#fff;border-radius:10px;cursor:crosshair;touch-action:none;box-shadow:0 1px 4px rgba(44,36,32,0.06)}
    #profileInfo{font-size:0.72em;color:#b0a898;text-align:center;margin-top:6px;font-family:'Courier New',monospace}
    .links{text-align:center;margin-top:18px}
    .links a{color:#b0a898;font-size:0.68em;text-decoration:none;text-transform:uppercase;letter-spacing:2px;font-family:-apple-system,BlinkMacSystemFont,sans-serif;transition:color .2s}
    .links a:hover{color:#e85d3a}
    .actual-info{font-size:0.72em;color:#b0a898;font-family:'Courier New',monospace;margin-top:2px}
    .hist-table{width:100%;border-collapse:collapse;font-size:0.75em;font-family:'Courier New',monospace}
    .hist-table th{text-align:left;color:#b0a898;font-weight:400;padding:4px 6px;border-bottom:1px solid #e8e2da;font-size:0.85em;text-transform:uppercase;letter-spacing:0.5px}
    .hist-table td{padding:4px 6px;border-bottom:1px solid #f0ece6;color:#5c524a}
    .hist-table tr:hover td{background:#faf8f5}
  </style>
</head>
<body>
  <div class="hdr"><h1>Coffee Scale</h1><small>Rancilio Silvia</small></div>

  <div class="status">
    <div class="status-item"><div class="status-label">Shots</div><div class="status-value" id="shots">--</div></div>
    <div class="status-item"><div class="status-label">Last</div><div class="status-value" id="lastTime">--</div></div>
    <div class="status-item"><div class="status-label">Sleep</div><div class="status-value" id="sleep">--</div></div>
    <div class="status-item"><div class="status-label">IP</div><div class="status-value" id="ip">--</div></div>
  </div>
  <div id="feedback" class="feedback"></div>

  <div class="section">
    <div class="section-title">Shot Profile</div>
    <canvas id="profileChart" width="440" height="200"></canvas>
    <div id="profileInfo"></div>
  </div>

  <div class="section">
    <div class="section-title">Dose Targets</div>
    <div class="setting"><label>Single dose</label><input id="singleDose" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleDose','singleDose')">Set</button></div>
    <div class="setting"><label>Double dose</label><input id="doubleDose" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleDose','doubleDose')">Set</button></div>
  </div>

  <div class="section">
    <div class="section-title">Overshoot</div>
    <div class="setting"><label>Single naked<div class="actual-info" id="actualSNInfo"></div></label><input id="singleNaked" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleNaked','singleNaked')">Set</button></div>
    <div class="setting"><label>Single spouted<div class="actual-info" id="actualSSInfo"></div></label><input id="singleSpouted" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleSpouted','singleSpouted')">Set</button></div>
    <div class="setting"><label>Double naked<div class="actual-info" id="actualDNInfo"></div></label><input id="doubleNaked" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleNaked','doubleNaked')">Set</button></div>
    <div class="setting"><label>Double spouted<div class="actual-info" id="actualDSInfo"></div></label><input id="doubleSpouted" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleSpouted','doubleSpouted')">Set</button></div>
  </div>

  <div class="section">
    <div class="section-title">Extraction</div>
    <div class="setting"><label>Correction time</label><input id="correctionTime" type="number" step="0.1" min="0" max="10"><span class="unit">s</span><button onclick="save('setCorrectionTime','correctionTime')">Set</button></div>
  </div>

  <div class="section">
    <div class="section-title">History</div>
    <div id="historyTable"></div>
  </div>

  <div class="links"><a href="/log">View Log</a></div>

  <script>
    function save(cmd, id) {
      var g = parseFloat(document.getElementById(id).value);
      if (isNaN(g)) return;
      var mg = Math.round(g * 1000);
      fetch('/api/send?cmd=' + cmd + ':' + mg).then(function(r) { return r.text(); }).then(function(t) {
        var fb = document.getElementById('feedback');
        fb.textContent = t;
        fb.classList.remove('show');
        void fb.offsetWidth;
        fb.classList.add('show');
        setTimeout(function() { fb.textContent = ''; fb.classList.remove('show'); }, 2000);
      });
    }
    function updateStatus() {
      fetch('/api/status').then(function(r) { return r.json(); }).then(function(d) {
        document.getElementById('shots').textContent = d.shotCount;
        document.getElementById('lastTime').textContent = (d.lastShotTime / 1000).toFixed(1) + 's';
        document.getElementById('sleep').textContent = d.sleep ? 'Yes' : 'No';
        document.getElementById('ip').textContent = d.ip;
        var fields = ['singleDose','doubleDose','singleNaked','singleSpouted','doubleNaked','doubleSpouted','correctionTime'];
        fields.forEach(function(id) {
          var el = document.getElementById(id);
          if (document.activeElement !== el) {
            el.value = (d[id] / 1000).toFixed(1);
          }
        });
        var actuals = [
          {id:'actualSNInfo', key:'actualSingleNaked'},
          {id:'actualSSInfo', key:'actualSingleSpouted'},
          {id:'actualDNInfo', key:'actualDoubleNaked'},
          {id:'actualDSInfo', key:'actualDoubleSpouted'}
        ];
        actuals.forEach(function(a) {
          var el = document.getElementById(a.id);
          var val = d[a.key];
          el.textContent = val > 0 ? 'Actual avg: ' + (val / 1000).toFixed(1) + 'g' : '';
        });
      }).catch(function() {});
    }
    var chartData = null;
    var chartMaxT = 0, chartMaxW = 0;
    var chartPad = {l:40, r:10, t:10, b:25};
    var chartRates = [], chartRatesSmooth = [], chartMaxRate = 0;
    var chartW = 440, chartH = 200;
    // Scale canvas for high-DPI displays
    (function() {
      var c = document.getElementById('profileChart');
      var dpr = window.devicePixelRatio || 1;
      c.width = chartW * dpr;
      c.height = chartH * dpr;
    })();

    function renderChart(hoverIdx) {
      var c = document.getElementById('profileChart');
      var ctx = c.getContext('2d');
      var dpr = window.devicePixelRatio || 1;
      var w = chartW, h = chartH;
      var pad = chartPad;
      var data = chartData;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, w, h);
      if (!data || !data.length) return;

      var plotW = w - pad.l - pad.r;
      var plotH = h - pad.t - pad.b;

      // Grid lines
      ctx.strokeStyle = '#ede8e2';
      ctx.lineWidth = 1;
      ctx.setLineDash([2, 4]);
      for (var g = 1; g <= 3; g++) {
        var gy = pad.t + (plotH * g / 4);
        ctx.beginPath(); ctx.moveTo(pad.l, gy); ctx.lineTo(w - pad.r, gy); ctx.stroke();
      }
      for (var g = 1; g <= 3; g++) {
        var gx = pad.l + (plotW * g / 4);
        ctx.beginPath(); ctx.moveTo(gx, pad.t); ctx.lineTo(gx, h - pad.b); ctx.stroke();
      }
      ctx.setLineDash([]);

      // Axes
      ctx.strokeStyle = '#d4cdc4';
      ctx.beginPath();
      ctx.moveTo(pad.l, pad.t);
      ctx.lineTo(pad.l, h - pad.b);
      ctx.lineTo(w - pad.r, h - pad.b);
      ctx.stroke();

      // Labels
      ctx.fillStyle = '#b0a898';
      ctx.font = '10px "Courier New",monospace';
      ctx.textAlign = 'right';
      ctx.fillText(chartMaxW.toFixed(0) + 'g', pad.l - 4, pad.t + 10);
      ctx.fillText('0', pad.l - 4, h - pad.b);
      ctx.textAlign = 'center';
      ctx.fillText('0s', pad.l, h - pad.b + 14);
      ctx.fillText((chartMaxT / 1000).toFixed(0) + 's', w - pad.r, h - pad.b + 14);

      // Weight fill
      var grad = ctx.createLinearGradient(0, pad.t, 0, h - pad.b);
      grad.addColorStop(0, 'rgba(232, 93, 58, 0.15)');
      grad.addColorStop(1, 'rgba(232, 93, 58, 0)');
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.moveTo(pad.l + (data[0].t / chartMaxT) * plotW, h - pad.b);
      for (var i = 0; i < data.length; i++) {
        var x = pad.l + (data[i].t / chartMaxT) * plotW;
        var y = (h - pad.b) - (data[i].w / chartMaxW) * plotH;
        ctx.lineTo(x, y);
      }
      ctx.lineTo(pad.l + (data[data.length-1].t / chartMaxT) * plotW, h - pad.b);
      ctx.closePath();
      ctx.fill();

      // Weight line
      ctx.strokeStyle = '#e85d3a';
      ctx.lineWidth = 2.5;
      ctx.beginPath();
      for (var i = 0; i < data.length; i++) {
        var x = pad.l + (data[i].t / chartMaxT) * plotW;
        var y = (h - pad.b) - (data[i].w / chartMaxW) * plotH;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();

      // Flow rate line (smoothed for display)
      if (chartRatesSmooth.length > 0 && chartMaxRate > 0) {
        ctx.strokeStyle = 'rgba(42, 166, 148, 0.55)';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (var i = 0; i < chartRatesSmooth.length; i++) {
          var x = pad.l + (data[i+1].t / chartMaxT) * plotW;
          var y = (h - pad.b) - (chartRatesSmooth[i] / chartMaxRate) * plotH * 0.5;
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      // Hover overlay
      if (hoverIdx >= 0 && hoverIdx < data.length) {
        var px = pad.l + (data[hoverIdx].t / chartMaxT) * plotW;
        var py = (h - pad.b) - (data[hoverIdx].w / chartMaxW) * plotH;

        ctx.setLineDash([3, 3]);
        ctx.strokeStyle = 'rgba(232,93,58,0.25)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(px, pad.t);
        ctx.lineTo(px, h - pad.b);
        ctx.stroke();
        ctx.setLineDash([]);

        // Dot on weight curve
        ctx.fillStyle = '#e85d3a';
        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 1.5;
        ctx.stroke();

        // Dot on flow rate curve
        var rateIdx = hoverIdx - 1;
        var hasRate = rateIdx >= 0 && rateIdx < chartRates.length && chartMaxRate > 0;
        if (hasRate) {
          var ry = (h - pad.b) - (chartRatesSmooth[rateIdx] / chartMaxRate) * plotH * 0.5;
          ctx.fillStyle = '#2aa694';
          ctx.beginPath();
          ctx.arc(px, ry, 3, 0, Math.PI * 2);
          ctx.fill();
        }

        // Tooltip
        var weightLabel = (data[hoverIdx].t / 1000).toFixed(1) + 's  ' + data[hoverIdx].w.toFixed(1) + 'g';
        var rateLabel = hasRate ? chartRatesSmooth[rateIdx].toFixed(1) + ' g/s' : '';
        ctx.font = '11px "Courier New",monospace';
        var tw = Math.max(ctx.measureText(weightLabel).width, rateLabel ? ctx.measureText(rateLabel).width : 0);
        var lh = rateLabel ? 30 : 16;
        var lx = px + 8;
        var ly = py - 10;
        if (lx + tw + 8 > w) lx = px - tw - 14;
        if (ly - 14 < 0) ly = py + 18;
        ctx.fillStyle = 'rgba(255,255,255,0.95)';
        ctx.fillRect(lx - 4, ly - 13, tw + 8, lh);
        ctx.strokeStyle = '#e8e2da';
        ctx.lineWidth = 1;
        ctx.strokeRect(lx - 4, ly - 13, tw + 8, lh);
        ctx.textAlign = 'left';
        ctx.fillStyle = '#e85d3a';
        ctx.fillText(weightLabel, lx, ly);
        if (rateLabel) {
          ctx.fillStyle = '#2aa694';
          ctx.fillText(rateLabel, lx, ly + 14);
        }
      }
    }

    function findNearestPoint(mouseX) {
      var c = document.getElementById('profileChart');
      var rect = c.getBoundingClientRect();
      var scaleX = chartW / rect.width;
      var canvasX = (mouseX - rect.left) * scaleX;
      var pad = chartPad;
      var plotW = chartW - pad.l - pad.r;
      var relX = canvasX - pad.l;
      if (relX < 0 || relX > plotW || !chartData) return -1;
      var targetT = (relX / plotW) * chartMaxT;
      var lo = 0, hi = chartData.length - 1;
      while (lo < hi) {
        var mid = (lo + hi) >> 1;
        if (chartData[mid].t < targetT) lo = mid + 1;
        else hi = mid;
      }
      if (lo > 0 && Math.abs(chartData[lo-1].t - targetT) < Math.abs(chartData[lo].t - targetT)) lo--;
      return lo;
    }

    var profileCanvas = document.getElementById('profileChart');
    profileCanvas.addEventListener('mousemove', function(e) {
      var idx = findNearestPoint(e.clientX);
      renderChart(idx);
    });
    profileCanvas.addEventListener('mouseleave', function() {
      renderChart(-1);
    });
    profileCanvas.addEventListener('touchmove', function(e) {
      e.preventDefault();
      var idx = findNearestPoint(e.touches[0].clientX);
      renderChart(idx);
    });
    profileCanvas.addEventListener('touchend', function() {
      renderChart(-1);
    });

    function drawProfile() {
      fetch('/api/profile').then(function(r) { return r.json(); }).then(function(resp) {
        var data = resp.profile || resp;
        var stats = resp.stats || null;
        if (!data || !data.length) {
          chartData = null;
          document.getElementById('profileInfo').textContent = 'No shot data yet';
          return;
        }
        chartData = data;
        chartMaxT = data[data.length-1].t;
        var minW = 0;
        for (var i = 0; i < data.length; i++) { if (data[i].w < minW) minW = data[i].w; }
        if (minW < 0) {
          for (var i = 0; i < data.length; i++) { data[i].w -= minW; }
        }
        chartMaxW = 0;
        for (var i = 0; i < data.length; i++) { if (data[i].w > chartMaxW) chartMaxW = data[i].w; }
        chartMaxW = chartMaxW * 1.15;
        if (chartMaxW < 1) chartMaxW = 1;

        // Pre-compute flow rates (raw for tooltips, smoothed for rendering)
        chartRates = [];
        chartRatesSmooth = [];
        chartMaxRate = 0;
        if (data.length > 2) {
          for (var i = 1; i < data.length; i++) {
            var dt = (data[i].t - data[i-1].t) / 1000;
            var rate = dt > 0 ? (data[i].w - data[i-1].w) / dt : 0;
            if (rate < 0) rate = 0;
            chartRates.push(rate);
          }
          var win = 2;
          for (var i = 0; i < chartRates.length; i++) {
            var sum = 0, cnt = 0;
            for (var j = Math.max(0, i - win); j <= Math.min(chartRates.length - 1, i + win); j++) {
              sum += chartRates[j]; cnt++;
            }
            var s = sum / cnt;
            chartRatesSmooth.push(s);
            if (s > chartMaxRate) chartMaxRate = s;
          }
        }

        renderChart(-1);

        var finalW = data[data.length-1].w;
        var totalT = (data[data.length-1].t / 1000).toFixed(1);
        var info = finalW.toFixed(1) + 'g in ' + totalT + 's';
        if (stats) {
          info += ' | Avg ' + stats.avgFlow.toFixed(1) + ' g/s';
          info += ' | Peak ' + stats.peakFlow.toFixed(1) + ' g/s';
          info += ' | First drip ' + stats.firstDrip.toFixed(1) + 's';
        }
        document.getElementById('profileInfo').textContent = info;
      }).catch(function() {
        chartData = null;
        document.getElementById('profileInfo').textContent = 'No shot data yet';
      });
    }
    function loadHistory() {
      fetch('/api/history').then(function(r) { return r.json(); }).then(function(data) {
        var el = document.getElementById('historyTable');
        if (!data || !data.length) {
          el.textContent = 'No history yet';
          return;
        }
        var html = '<table class="hist-table"><tr><th>#</th><th>Weight</th><th>Time</th><th>Avg</th><th>Peak</th><th>Dose</th><th>Reason</th></tr>';
        for (var i = 0; i < data.length; i++) {
          var s = data[i];
          html += '<tr><td>' + s.n + '</td>';
          html += '<td>' + s.w.toFixed(1) + 'g</td>';
          html += '<td>' + s.t.toFixed(1) + 's</td>';
          html += '<td>' + s.avgFlow.toFixed(1) + '</td>';
          html += '<td>' + s.peakFlow.toFixed(1) + '</td>';
          html += '<td>' + s.dose + '</td>';
          html += '<td>' + (s.reason === 'target_reached' ? '\u2713' : 'stop') + '</td></tr>';
        }
        html += '</table>';
        el.innerHTML = html;
      }).catch(function() {
        document.getElementById('historyTable').textContent = 'No history yet';
      });
    }
    updateStatus();
    drawProfile();
    loadHistory();
    setInterval(updateStatus, 3000);
  </script>
</body>
</html>
)=====";

#endif // WEB_H
