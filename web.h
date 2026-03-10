#ifndef WEB_H
#define WEB_H

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Coffee Scale</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; padding: 16px; max-width: 480px; margin: 0 auto; }
    h1 { font-size: 1.3em; text-align: center; margin-bottom: 14px; color: #e94560; letter-spacing: 0.5px; }
    .status { display: flex; flex-wrap: wrap; justify-content: space-around; margin-bottom: 16px; padding: 14px; background: rgba(22, 33, 62, 0.7); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px); border-radius: 12px; box-shadow: 0 4px 16px rgba(0,0,0,0.25), inset 0 1px 0 rgba(255,255,255,0.05); gap: 8px; border: 1px solid rgba(255,255,255,0.06); }
    .status-item { text-align: center; min-width: 80px; padding: 4px 10px; border-radius: 20px; background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.05); }
    .status-label { font-size: 0.65em; color: #888; text-transform: uppercase; letter-spacing: 0.5px; }
    .status-value { font-size: 1.1em; font-weight: 600; margin-top: 2px; }
    .section { margin-bottom: 16px; }
    .section-title { font-size: 0.75em; color: #888; text-transform: uppercase; letter-spacing: 1.2px; margin-bottom: 8px; padding-left: 8px; border-left: 2px solid #e94560; }
    .setting { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; padding: 10px 12px; background: rgba(22, 33, 62, 0.7); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px); border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.2), inset 0 1px 0 rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.05); transition: border-color 0.2s; }
    .setting:hover { border-color: rgba(233, 69, 96, 0.2); }
    .setting label { flex: 1; font-size: 0.85em; }
    .setting input { width: 72px; background: rgba(15, 52, 96, 0.8); color: #eee; border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 8px; text-align: right; font-size: 0.9em; transition: border-color 0.2s, box-shadow 0.2s; }
    .setting input:focus { border-color: #e94560; outline: none; box-shadow: 0 0 8px rgba(233, 69, 96, 0.3); }
    .setting input:hover { border-color: rgba(233, 69, 96, 0.4); }
    .unit { font-size: 0.75em; color: #888; width: 12px; }
    button { background: linear-gradient(135deg, #0f3460, #16213e); color: #eee; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px 14px; font-size: 0.8em; cursor: pointer; transition: all 0.2s; box-shadow: 0 2px 6px rgba(0,0,0,0.2); }
    button:active { background: linear-gradient(135deg, #e94560, #c73652); transform: scale(0.95); }
    button:hover { border-color: #e94560; box-shadow: 0 2px 10px rgba(233, 69, 96, 0.25); }
    .feedback { text-align: center; padding: 8px; font-size: 0.8em; color: #4ecca3; min-height: 24px; }
    .feedback.show { animation: fadeInUp 0.3s ease; }
    @keyframes fadeInUp { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }
    .links { text-align: center; margin-top: 14px; font-size: 0.8em; }
    .links a { color: #4ecca3; margin: 0 8px; text-decoration: none; transition: color 0.2s; }
    .links a:hover { color: #6fe8c0; }
    canvas { width: 100%; background: rgba(22, 33, 62, 0.7); border-radius: 10px; cursor: crosshair; touch-action: none; border: 1px solid rgba(255,255,255,0.05); box-shadow: 0 2px 10px rgba(0,0,0,0.2); }
  </style>
</head>
<body>
  <h1>Coffee Scale</h1>

  <div class="status">
    <div class="status-item"><div class="status-label">Shots</div><div class="status-value" id="shots">--</div></div>
    <div class="status-item"><div class="status-label">Last time</div><div class="status-value" id="lastTime">--</div></div>
    <div class="status-item"><div class="status-label">Sleep</div><div class="status-value" id="sleep">--</div></div>
    <div class="status-item"><div class="status-label">IP</div><div class="status-value" id="ip">--</div></div>
  </div>
  <div id="feedback" class="feedback"></div>

  <div class="section">
    <div class="section-title">Last Shot Profile</div>
    <canvas id="profileChart" width="440" height="200"></canvas>
    <div id="profileInfo" style="font-size:0.75em;color:#888;text-align:center;margin-top:4px;"></div>
  </div>

  <div class="section">
    <div class="section-title">Dose Targets</div>
    <div class="setting"><label>Single dose</label><input id="singleDose" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleDose','singleDose')">Save</button></div>
    <div class="setting"><label>Double dose</label><input id="doubleDose" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleDose','doubleDose')">Save</button></div>
  </div>

  <div class="section">
    <div class="section-title">Overshoot (post-flow weight)</div>
    <div class="setting"><label>Single naked</label><input id="singleNaked" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleNaked','singleNaked')">Save</button></div>
    <div class="setting"><label>Single spouted</label><input id="singleSpouted" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setSingleSpouted','singleSpouted')">Save</button></div>
    <div class="setting"><label>Double naked</label><input id="doubleNaked" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleNaked','doubleNaked')">Save</button></div>
    <div class="setting"><label>Double spouted</label><input id="doubleSpouted" type="number" step="0.1"><span class="unit">g</span><button onclick="save('setDoubleSpouted','doubleSpouted')">Save</button></div>
  </div>

  <div class="section">
    <div class="section-title">Extraction</div>
    <div class="setting"><label>Correction time</label><input id="correctionTime" type="number" step="0.1" min="0" max="10"><span class="unit">s</span><button onclick="save('setCorrectionTime','correctionTime')">Save</button></div>
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
      }).catch(function() {});
    }
    // Chart state shared between render and hover
    var chartData = null;
    var chartMaxT = 0, chartMaxW = 0;
    var chartPad = {l:40, r:10, t:10, b:25};
    var chartRates = [], chartMaxRate = 0;

    function renderChart(hoverIdx) {
      var c = document.getElementById('profileChart');
      var ctx = c.getContext('2d');
      var w = c.width, h = c.height;
      var pad = chartPad;
      var data = chartData;
      ctx.clearRect(0, 0, w, h);
      if (!data || !data.length) return;

      var plotW = w - pad.l - pad.r;
      var plotH = h - pad.t - pad.b;

      // Axes
      ctx.strokeStyle = '#333';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(pad.l, pad.t);
      ctx.lineTo(pad.l, h - pad.b);
      ctx.lineTo(w - pad.r, h - pad.b);
      ctx.stroke();

      // Labels
      ctx.fillStyle = '#888';
      ctx.font = '11px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(chartMaxW.toFixed(0) + 'g', pad.l - 4, pad.t + 10);
      ctx.fillText('0', pad.l - 4, h - pad.b);
      ctx.textAlign = 'center';
      ctx.fillText('0s', pad.l, h - pad.b + 14);
      ctx.fillText((chartMaxT / 1000).toFixed(0) + 's', w - pad.r, h - pad.b + 14);

      // Weight fill (gradient under line)
      var grad = ctx.createLinearGradient(0, pad.t, 0, h - pad.b);
      grad.addColorStop(0, 'rgba(233, 69, 96, 0.25)');
      grad.addColorStop(1, 'rgba(233, 69, 96, 0)');
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
      ctx.strokeStyle = '#e94560';
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (var i = 0; i < data.length; i++) {
        var x = pad.l + (data[i].t / chartMaxT) * plotW;
        var y = (h - pad.b) - (data[i].w / chartMaxW) * plotH;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();

      // Flow rate line
      if (chartRates.length > 0 && chartMaxRate > 0) {
        ctx.strokeStyle = 'rgba(78, 204, 163, 0.5)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        for (var i = 0; i < chartRates.length; i++) {
          var x = pad.l + (data[i+1].t / chartMaxT) * plotW;
          var y = (h - pad.b) - (chartRates[i] / chartMaxRate) * plotH * 0.5;
          if (i === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      // Hover overlay
      if (hoverIdx >= 0 && hoverIdx < data.length) {
        var px = pad.l + (data[hoverIdx].t / chartMaxT) * plotW;
        var py = (h - pad.b) - (data[hoverIdx].w / chartMaxW) * plotH;

        // Vertical dashed line
        ctx.setLineDash([4, 3]);
        ctx.strokeStyle = 'rgba(255,255,255,0.3)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(px, pad.t);
        ctx.lineTo(px, h - pad.b);
        ctx.stroke();
        ctx.setLineDash([]);

        // Dot on weight curve
        ctx.fillStyle = '#e94560';
        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fill();

        // Dot on flow rate curve
        var rateIdx = hoverIdx - 1;
        var hasRate = rateIdx >= 0 && rateIdx < chartRates.length && chartMaxRate > 0;
        if (hasRate) {
          var ry = (h - pad.b) - (chartRates[rateIdx] / chartMaxRate) * plotH * 0.5;
          ctx.fillStyle = 'rgba(78, 204, 163, 0.8)';
          ctx.beginPath();
          ctx.arc(px, ry, 3, 0, Math.PI * 2);
          ctx.fill();
        }

        // Build label lines
        var weightLabel = (data[hoverIdx].t / 1000).toFixed(1) + 's: ' + data[hoverIdx].w.toFixed(1) + 'g';
        var rateLabel = hasRate ? chartRates[rateIdx].toFixed(1) + ' g/s' : '';
        ctx.font = '12px sans-serif';
        var tw = Math.max(ctx.measureText(weightLabel).width, rateLabel ? ctx.measureText(rateLabel).width : 0);
        var lh = rateLabel ? 32 : 16;
        var lx = px + 8;
        var ly = py - 10;
        // Clamp to canvas edges
        if (lx + tw + 6 > w) lx = px - tw - 14;
        if (ly - 14 < 0) ly = py + 18;
        ctx.fillStyle = 'rgba(15, 52, 96, 0.9)';
        ctx.fillRect(lx - 3, ly - 13, tw + 6, lh);
        ctx.textAlign = 'left';
        ctx.fillStyle = '#eee';
        ctx.fillText(weightLabel, lx, ly);
        if (rateLabel) {
          ctx.fillStyle = 'rgba(78, 204, 163, 0.8)';
          ctx.fillText(rateLabel, lx, ly + 14);
        }
      }
    }

    function findNearestPoint(mouseX) {
      var c = document.getElementById('profileChart');
      var rect = c.getBoundingClientRect();
      var scaleX = c.width / rect.width;
      var canvasX = (mouseX - rect.left) * scaleX;
      var pad = chartPad;
      var plotW = c.width - pad.l - pad.r;
      var relX = canvasX - pad.l;
      if (relX < 0 || relX > plotW || !chartData) return -1;
      var targetT = (relX / plotW) * chartMaxT;
      // Binary search for nearest point
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
      fetch('/api/profile').then(function(r) { return r.json(); }).then(function(data) {
        if (!data.length) {
          chartData = null;
          document.getElementById('profileInfo').textContent = 'No shot data yet';
          return;
        }
        chartData = data;
        chartMaxT = data[data.length-1].t;
        // If any values are negative, shift all weights up to re-zero the baseline
        var minW = 0;
        for (var i = 0; i < data.length; i++) { if (data[i].w < minW) minW = data[i].w; }
        if (minW < 0) {
          for (var i = 0; i < data.length; i++) { data[i].w -= minW; }
        }
        chartMaxW = 0;
        for (var i = 0; i < data.length; i++) { if (data[i].w > chartMaxW) chartMaxW = data[i].w; }
        chartMaxW = chartMaxW * 1.15;
        if (chartMaxW < 1) chartMaxW = 1;

        // Pre-compute flow rates
        chartRates = [];
        chartMaxRate = 0;
        if (data.length > 2) {
          for (var i = 1; i < data.length; i++) {
            var dt = (data[i].t - data[i-1].t) / 1000;
            var rate = dt > 0 ? (data[i].w - data[i-1].w) / dt : 0;
            if (rate < 0) rate = 0;
            chartRates.push(rate);
            if (rate > chartMaxRate) chartMaxRate = rate;
          }
        }

        renderChart(-1);

        var finalW = data[data.length-1].w;
        var totalT = (data[data.length-1].t / 1000).toFixed(1);
        document.getElementById('profileInfo').textContent = 'Final: ' + finalW.toFixed(1) + 'g in ' + totalT + 's';
      }).catch(function() {
        chartData = null;
        document.getElementById('profileInfo').textContent = 'No shot data yet';
      });
    }
    updateStatus();
    drawProfile();
    setInterval(updateStatus, 3000);
  </script>
</body>
</html>
)=====";

#endif // WEB_H
