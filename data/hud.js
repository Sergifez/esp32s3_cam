// ===============================
// ELEMENTOS
// ===============================
const img = document.getElementById('stream');
const canvas = document.getElementById('overlay');
const ctx = canvas.getContext('2d');

// ===============================
// HUD LOCAL (tu simulación)
// ===============================
let hud = {
  speed: 0,
  battery: 100,
  lap: 1,
  distance: 0,
  rivalDist: 0
};

let speed = 0;
let gear = 1;
let hp = 1200;

// ===============================
// SIMULACIÓN LOCAL (tu código tal cual)
// ===============================
function updateValues() {
    speed += 0.12;
    if (speed > 260) speed = 0;

    if (speed < 40) gear = 1;
    else if (speed < 80) gear = 2;
    else if (speed < 120) gear = 3;
    else if (speed < 160) gear = 4;
    else if (speed < 200) gear = 5;
    else gear = 6;

    hp = 3000 + Math.floor(Math.sin(speed * 0.03) * 1500);
}

// ===============================
// CANVAS
// ===============================
function resizeCanvas() {
  const rect = img.getBoundingClientRect();
  canvas.width = rect.width;
  canvas.height = rect.height;

  canvas.style.width = rect.width + "px";
  canvas.style.height = rect.height + "px";
  canvas.style.left = rect.left + "px";
  canvas.style.top = rect.top + "px";
}

window.addEventListener("load", resizeCanvas);
window.addEventListener("resize", resizeCanvas);
img.onload = resizeCanvas;

// ===============================
// DIBUJO DEL HUD (tu código tal cual)
// ===============================
function drawSpeedCircle() {
    const w = canvas.width;
    const h = canvas.height;

    const cx = w * 0.16;
    const cy = h * 0.51;
    const r = h * 0.09;

    ctx.save();
    ctx.shadowColor = "rgba(0, 200, 255, 1)";
    ctx.shadowBlur = h * 0.05;

    ctx.strokeStyle = "rgba(0, 200, 255, 0.8)";
    ctx.lineWidth = h * 0.005;

    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
}

function drawSpeedArc(speed, maxSpeed = 300) {
    const w = canvas.width;
    const h = canvas.height;

    const cx = w * 0.16;
    const cy = h * 0.51;
    const r = h * 0.09;

    const startAngle = 1.2 * Math.PI;
    const endAngle   = 2.8 * Math.PI;

    const progress = speed / maxSpeed;
    const currentAngle = startAngle + (endAngle - startAngle) * progress;

    ctx.save();
    ctx.shadowColor = "rgba(0, 200, 255, 0.9)";
    ctx.shadowBlur = h * 0.03;

    ctx.strokeStyle = "rgba(0, 200, 255, 0.9)";
    ctx.lineWidth = h * 0.02;
    ctx.lineCap = "round";

    ctx.beginPath();
    ctx.arc(cx, cy, r, startAngle, currentAngle);
    ctx.stroke();
    ctx.restore();
}

function drawSpeedometer(speed, gear, hp) {
    const w = canvas.width;
    const h = canvas.height;

    const x = w * 0.16;
    const y = h * 0.53;

    ctx.save();
    ctx.textAlign = "center";

    ctx.shadowColor = "rgba(0, 200, 255, 0.8)";
    ctx.shadowBlur = h * 0.02;

    ctx.fillStyle = "#ffffff";
    ctx.font = (h * 0.055) + "px Orbitron";
    ctx.fillText(speed.toFixed(0), x, y);

    ctx.font = (h * 0.02) + "px Orbitron";
    ctx.fillText("km/h", x, y + h * 0.03);

    ctx.fillStyle = "#ffcc00";
    ctx.font = (h * 0.05) + "px Orbitron";
    ctx.fillText(gear, x - w * 0.03, y + h * 0.12);

    ctx.fillStyle = "#00ffff";
    ctx.font = (h * 0.03) + "px Orbitron";
    ctx.fillText(hp + " HP", x - w * 0.03, y + h * 0.18);

    ctx.restore();
}

function drawHUD() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  updateValues();
  drawSpeedCircle();
  drawSpeedArc(speed);
  drawSpeedometer(speed, gear, hp);

  requestAnimationFrame(drawHUD);
}
drawHUD();

// ===============================
// TELEMETRÍA (AsyncWebServer)
// ===============================
async function updateTelemetry() {
  try {
    const r = await fetch("/telemetry", { cache: "no-store" });
    const data = await r.json();

    hud.speed     = data.speed;
    hud.battery   = data.battery;
    hud.lap       = data.lap;
    hud.distance  = data.distance;
    hud.rivalDist = data.rivalDist;

  } catch (e) {
    console.warn("Error telemetría:", e);
  }
}

setInterval(updateTelemetry, 200);

// ===============================
// LED
// ===============================
document.getElementById("btn-left").addEventListener("mousedown", () => {
    fetch("/led?color=red");
});

document.getElementById("btn-right").addEventListener("mousedown", () => {
    fetch("/led?color=green");
});
