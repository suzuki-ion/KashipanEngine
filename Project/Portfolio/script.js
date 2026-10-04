'use strict';
// Values transcribed from the supplied technical report, page 12.
const measurements = [
  {lights:64, forward:3.356, plus:3.114},
  {lights:128, forward:5.854, plus:2.990},
  {lights:256, forward:11.073, plus:3.195},
  {lights:512, forward:19.111, plus:4.201},
  {lights:1024, forward:37.263, plus:6.350}
];
const chart = document.querySelector('#chart');
if (chart) {
  for (const data of measurements) {
    const row = document.createElement('div');
    row.className = 'chart-row';
    const label = document.createElement('span');
    label.textContent = String(data.lights);
    const bars = document.createElement('div');
    bars.className = 'bars';
    for (const key of ['forward', 'plus']) {
      const bar = document.createElement('div');
      bar.className = `bar ${key}`;
      bar.style.setProperty('--width', `${data[key] / 37.263 * 100}%`);
      const value = document.createElement('span');
      value.textContent = data[key].toFixed(3);
      bar.append(value);
      bars.append(bar);
    }
    row.append(label, bars);
    chart.append(row);
  }
}
