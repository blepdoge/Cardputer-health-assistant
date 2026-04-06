#pragma once
#include <Arduino.h>

// This stores the entire webpage in the ESP32's flash memory!
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>M5 Health Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root { --accent: #009688; --bg: #121212; --card: #1e1e1e; --text: #ffffff; --subtext: #aaaaaa; }
        body { font-family: 'Segoe UI', system-ui, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 20px; }
        .container { max-width: 800px; margin: auto; }
        .card { background-color: var(--card); border-radius: 12px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
        h1, h2 { color: var(--accent); margin-top: 0; }
        .header-top { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #333; padding-bottom: 15px; margin-bottom: 15px; }
        .clock-container { text-align: right; }
        #clock { font-size: 1.8em; font-weight: bold; margin: 0; }
        #date, #ssid-display { color: var(--subtext); font-size: 0.9em; }

        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: var(--subtext); }
        input { width: 100%; padding: 10px; background: #2c2c2c; border: 1px solid #444; color: white; border-radius: 6px; box-sizing: border-box; font-size: 1em; }
        input:focus { outline: none; border-color: var(--accent); }
        button { background-color: var(--accent); color: white; border: none; padding: 12px; border-radius: 6px; width: 100%; font-size: 1.1em; cursor: pointer; font-weight: bold; transition: 0.2s;}
        button:hover { background-color: #00796b; }

        canvas { max-width: 100%; margin-top: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <div class="header-top">
                <div>
                    <h1>Health Dashboard</h1>
                    <div id="ssid-display">Network: Loading...</div>
                </div>
                <div class="clock-container">
                    <div id="clock">--:--</div>
                    <div id="date">--/--/----</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Activity Log</h2>
            <canvas id="activityChart"></canvas>
        </div>

        <div class="card">
            <h2>Device Settings</h2>
            <form id="settingsForm">
                <div class="form-group">
                    <label>Daily Goal (steps)</label>
                    <input type="number" id="goal" required>
                </div>
                <div class="form-group">
                    <label>Height (cm)</label>
                    <input type="number" step="0.1" id="height" required>
                </div>
                <div class="form-group">
                    <label>Weight (kg)</label>
                    <input type="number" step="0.1" id="weight" required>
                </div>
                <div class="form-group">
                    <label>UTC Offset (hours)</label>
                    <input type="number" id="tz" required>
                </div>
                <button type="submit" id="saveBtn">Save to Cardputer</button>
            </form>
        </div>
    </div>

    <script>
        // 1. Live Client-Side Clock
        setInterval(() => {
            const now = new Date();
            document.getElementById('clock').innerText = now.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
            document.getElementById('date').innerText = now.toLocaleDateString();
        }, 1000);

        // 2. Fetch Initial Settings & Network from ESP32
        fetch('/api/settings').then(res => res.json()).then(data => {
            document.getElementById('ssid-display').innerText = 'Network: ' + data.ssid;
            document.getElementById('goal').value = data.goal;
            document.getElementById('height').value = data.height;
            document.getElementById('weight').value = data.weight;
            document.getElementById('tz').value = data.tz;
        });

        // 3. Form Submission (POST to ESP32)
        document.getElementById('settingsForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const btn = document.getElementById('saveBtn');
            btn.innerText = "Saving...";

            const payload = new URLSearchParams({
                goal: document.getElementById('goal').value,
                height: document.getElementById('height').value,
                weight: document.getElementById('weight').value,
                tz: document.getElementById('tz').value
            });

            fetch('/api/settings', { method: 'POST', body: payload })
                .then(() => {
                    btn.innerText = "Saved!";
                    btn.style.backgroundColor = "#4CAF50"; // Turn green
                    setTimeout(() => { btn.innerText = "Save to Cardputer"; btn.style.backgroundColor = ""; }, 2000);
                });
        });

        // 4. Fetch CSV & Render Chart.js Graph
        fetch('/api/data.csv').then(res => res.text()).then(csv => {
            const lines = csv.trim().split('\n');
            const labels = [];
            const dataPoints = [];

            // Start at 1 to skip the "Time,StepDelta" header row
            for(let i = 1; i < lines.length; i++) {
                const cols = lines[i].split(',');
                if(cols.length >= 2) {
                    labels.push(cols[0]);
                    dataPoints.push(parseInt(cols[1]));
                }
            }

            new Chart(document.getElementById('activityChart'), {
                type: 'bar',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Steps Taken',
                        data: dataPoints,
                        backgroundColor: '#009688',
                        borderRadius: 4
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        y: { beginAtZero: true, grid: { color: '#333' } },
                        x: { grid: { display: false } }
                    },
                    plugins: { legend: { display: false } }
                }
            });
        }).catch(err => console.log("No CSV data yet or read error."));
    </script>
</body>
</html>
)=====";
