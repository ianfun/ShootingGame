window.onerror = alert;
function runGame(player_name){
var running = true;
function quitGame(){
	running = false;
}
const KEY_DWON = 40;
const KEY_UP = 38;
const KEY_LEFT = 37;
const KEY_RIGHT = 39;
const KEY_SPEED_UP = 90;
const MAX_SPEED = 10;
const MIN_SPEED = 5;
const BULLET_SPEED = 40;
const my_x = 725;
const my_y = 325;
const WS_UPDATE_CLOCK = 1;
var bg_x = 0;
var bg_y = 0;
var speed = 1;
var rdegree = 0.0;
var then;
var bulletSet = new Set();
var wsClock = 0;
var blood = 100;
var quit = document.createElement('button');
var players = {};
quit.innerText = 'quit';
quit.onclick = quitGame;
quit.style.position = 'fixed';
quit.style.left = '5px';
quit.style.top = '5px';
document.body.appendChild(quit);
var canvas = document.createElement('canvas');
canvas.width = 1500;
canvas.height = 700;
canvas.style.top = '50px';
canvas.style.left = '50px';
document.body.appendChild(canvas);
var ctx = canvas.getContext('2d');
var isMousePressed = false;
ctx.font  = "30px Comic Sans"
ctx.fillStyle = "orange";
const keyMap = new Array(255);
var counter = 0;
var gradient = ctx.createConicGradient(0, 25, 25);
gradient.addColorStop(0, 'yellow');
gradient.addColorStop(.5, 'orange');
gradient.addColorStop(1, 'red');
var ws = new WebSocket('ws://' + location.host, encodeURIComponent(player_name));
ws.binaryType = 'arraybuffer';
ws.onerror = ws.onclose = quitGame;
ws.onopen = function(){
	ws.onmessage = function(m){
		var view = new DataView(m.data);
		var old = players;
		players = {};
		for(var i=0;i<m.data.byteLength;i+=8){
			const id = view.getInt16(i, true);
			const x  = view.getInt16(i+2, true) / 15;
			const y  = view.getInt16(i+4, true) / 15;
			const rad= view.getInt16(i+6, true) / 10000;
			const last = old[id];
			if (last) {
				players[id] = {
					r: 0, 
					xs: (x - last.x) / WS_UPDATE_CLOCK, 
					ys: (y - last.y) / WS_UPDATE_CLOCK, 
				};
			}else{
				players[id] = {};
			}
			players[id].x = x;
			players[id].y = y;
			players[id].rad = rad;
		}
	};
	const DOWN_LIMIT = my_y-bg.height;
	const RIGHT_LIMIT = my_x - bg.width;
	addEventListener('keydown', function(e){
		keyMap[e.keyCode] = true;
	});
	addEventListener('keyup', function(e){
		keyMap[e.keyCode] = false;
	});
	addEventListener('mousemove', function(e) {
		var deltaX = e.x - (50 + my_x);
		var deltaY = e.y - (50 + my_y);
		rdegree = Math.atan2(deltaY, deltaX);
	});
	addEventListener("mousedown", function(e){
		isMousePressed = true;
	});
	addEventListener('mouseup', function(e){
		isMousePressed = false;
	});
	function Bullet(){
		this.x = -bg_x + my_x + Math.cos(rdegree)*90;
		this.y = -bg_y + my_y + Math.sin(rdegree)*90;
		this.rad = rdegree;
	}
	function frame(){
		++wsClock;
		if (wsClock >= WS_UPDATE_CLOCK) {
			if(ws.bufferedAmount==0){
				wsClock = 0;
				var view = new DataView(new ArrayBuffer(6));
				view.setInt16(0, (-bg_x + my_x)*15, true);
				view.setInt16(2, (-bg_y + my_y)*15, true);
				view.setInt16(4, rdegree*10000, true);
				// range of uint16 is 0~65535, uint16 is 32768~32767
				// rad's range is -1.57~1.57
				// range of image is in -1000 ~ 1000
				ws.send(view.buffer);
			}
		}
	if(speed > MIN_SPEED){
		--speed;
	}
	if(keyMap[KEY_DWON] || keyMap[KEY_UP] || keyMap[KEY_RIGHT] || keyMap[KEY_LEFT]){
		if(keyMap[KEY_SPEED_UP]){
			speed+=8;
		}
		if(speed > MAX_SPEED){
			speed = MAX_SPEED;
		}
		if(keyMap[KEY_DWON]){
			bg_y-=speed;
		}
		if(keyMap[KEY_UP]){
			bg_y+=speed;
		}
		if(keyMap[KEY_RIGHT]){
			bg_x-=speed;
		}
		if(keyMap[KEY_LEFT]){
			bg_x+=speed;
		}
	}else{
		if(isMousePressed){
			const r = keyMap[KEY_SPEED_UP] ? MAX_SPEED : MIN_SPEED;
			const x = Math.cos(rdegree) * r;
			const y = Math.sin(rdegree) * r;
			bg_x -= x;
			bg_y -= y;
		}
	}
		if(bg_y > my_y){
			bg_y = my_y;
		}else if (bg_y < DOWN_LIMIT){
			bg_y = DOWN_LIMIT;
		}
		if (bg_x > my_x) {
			bg_x = my_x;
		}else if (bg_x < RIGHT_LIMIT){
			bg_x = RIGHT_LIMIT;
		}
		for(let bullet of bulletSet){
			if(bullet.x < 0 || bullet.y < 0){
				bulletSet.delete(bullet);
				continue;
			}
			if (bullet.y > bg.height || bullet.x > bg.width) {
				bulletSet.delete(bullet);
				continue;
			}
		}
		if (keyMap[32]) {
			if(counter==0){bulletSet.add(new Bullet());}
			if (counter > 5) {counter = 0}
			else { ++counter ;}
		}else{
			counter = 0;
		}
		key = 0;
		draw();

	}
	function drawPlayer(x, y, deg){
		const CENTERX = 10;
		const CENTERY = 24;
		ctx.save();
		ctx.translate(x, y);
		ctx.rotate(deg);

		ctx.strokeStyle = gradient;
		ctx.lineWidth = 4;
		ctx.beginPath();
		ctx.arc(CENTERX, CENTERY, 50, 0, 2 * Math.PI);
		ctx.stroke();

		ctx.fillStyle = 'silver';
		ctx.fillRect(0, 0, 20, 50);
		ctx.fillRect(0, 0, 100, 10);
		
		ctx.fillStyle = 'black';
		ctx.beginPath();
		ctx.arc(CENTERX, CENTERY, 17, 0, (2 * Math.PI) * (blood/100));
		ctx.fill();

		ctx.restore();
	}
	function draw(){
		// ctx.strokeStyle = 'black';
		ctx.lineWidth = 1;
		ctx.clearRect(0, 0, canvas.width, canvas.height);
		ctx.drawImage(bg, bg_x, bg_y, bg.width, bg.height);

		drawPlayer(my_x, my_y, rdegree);

		ctx.lineWidth = 3;
		for(let bullet of bulletSet){
			ctx.beginPath();
			bullet.x += Math.cos(bullet.rad) * BULLET_SPEED;
			bullet.y += Math.sin(bullet.rad) * BULLET_SPEED;
			ctx.arc(bullet.x + bg_x, bullet.y + bg_y, 3.5, 0, 2*Math.PI);
			ctx.stroke();
		}
		for(let p of Object.values(players)){
			if (p.r!=undefined) {
				const r = p.r;
				drawPlayer(bg_x + p.x + (p.xs * r), bg_y + p.y + (p.ys * r), p.rad);
				++p.r;
			}else{
				drawPlayer(bg_x + p.x, bg_y + p.y, p.rad);
			}
		}
	}
	function fpsDraw(now){
		if(running){
		requestAnimationFrame(fpsDraw);
			if ((now - then) > 30) {
				then = now;
				frame();
			}
		}else{
			document.body.removeChild(canvas);
			document.body.removeChild(quit);
			ws.close(1000, '');
			document.body.appendChild(gs);
		}
	}
	then = performance.now()
	requestAnimationFrame(fpsDraw);
}
}