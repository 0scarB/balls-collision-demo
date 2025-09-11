"use strict";

const PRINT_STRING            = 0;
const THROW_ERROR_FROM_STRING = 1;

/** @type {WebAssembly.Instance} **/
var wasmInstance;
/** @type {Uint8Array} **/
var wasmMemoryUint8View;
/** @type {Float32Array} **/
var wasmMemoryFloat32View;
/** @type {Uint32Array} **/
var wasmMemoryUint32View;
var wasmTextOutput = "";
var wasmStarted    = false;
var wasmLoaded     = false;
var pageLoaded     = false;

/** @type {CanvasRenderingContext2D} **/
var canvasContext2d;
var canvasWidth  = -1;
var canvasHeight = -1;
/** @type {WebGL2RenderingContext} **/
var gl;

function doUpdateLoop() {
    wasmInstance.exports.update(performance.now());
    requestAnimationFrame(doUpdateLoop);
}

function start() {
    /** @type {HTMLCanvasElement} **/
    var canvasElement2d = document.getElementById("canvas-2d");
    canvasWidth  = canvasElement2d.width;
    canvasHeight = canvasElement2d.height;
    canvasContext2d = canvasElement2d.getContext("2d");
    canvasContext2d.lineCap = "round";

    /** @type {HTMLCanvasElement} **/
    var canvasElementWebGL = document.getElementById("canvas-webgl2");
    gl = canvasElementWebGL.getContext("webgl2");
    if (gl.canvas.width  !== canvasWidth ||
        gl.canvas.height !== canvasHeight
    ) { throw new Error("All canvases in the test must have equal dimensions!"); }

    if (!gl) { throw new Error("Your browser does not support WebGL 2.0!"); }
    var vertexShaderSource =
        "#version 300 es\n"+
        "uniform vec2 ball_xy_mul;\n"+
        "in uvec3 packed_ball_data;\n"+
        "flat out uint packed_color;\n"+
        "void main() {\n"+
        "   gl_Position =\n"+
        "       vec4(uintBitsToFloat(packed_ball_data.x)*ball_xy_mul.x - 1.0,\n"+
        "            uintBitsToFloat(packed_ball_data.y)*ball_xy_mul.y + 1.0,\n"+
        "            0.0, 1.0);\n"+
        "   gl_PointSize = 2.0*float(packed_ball_data[2] & 255u);\n"+
        "   packed_color = packed_ball_data[2] >> 8;\n"+
        "}";
    var vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, vertexShaderSource);
    gl.compileShader(vertexShader);
    if (!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
        throw new Error("Failed to compile WebGL vertex shader!\n\n"+
            `Shader info log: ${gl.getShaderInfoLog(vertexShader)}`);
    }

    var fragmentShaderSource =
        "#version 300 es\n"+
        "precision highp float;\n"+
        "flat in uint packed_color;\n"+
        "out vec3 color;\n"+
        "void main() {\n"+
        "   vec2 pos_rel_to_point_center = gl_PointCoord - vec2(0.5, 0.5);\n"+
        "   if (dot(pos_rel_to_point_center, pos_rel_to_point_center)\n"+
        "       < 0.25\n"+
        "   ) {\n"+
        "       color = vec3(float(packed_color       & 255u)/256.0,\n"+
        "                    float(packed_color >>  8 & 255u)/256.0,\n"+
        "                    float(packed_color >> 16       )/256.0);\n"+
        "   }\n"+
        "}";
    var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, fragmentShaderSource);
    gl.compileShader(fragmentShader);
    if (!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS))
        throw new Error("Failed to compile WebGL fragment shader!\n\n"+
            `Shader info log: ${gl.getShaderInfoLog(fragmentShader)}`);

    var glProgram = gl.createProgram();
    gl.attachShader(glProgram, vertexShader);
    gl.attachShader(glProgram, fragmentShader);
    gl.linkProgram(glProgram);
    if (!gl.getProgramParameter(glProgram, gl.LINK_STATUS))
        { throw new Error("Failed to link WebGL program!"); }

    gl.useProgram(glProgram);

    var ballXYMulUniform = gl.getUniformLocation(glProgram, "ball_xy_mul");
    gl.uniform2f(ballXYMulUniform, 2.0/(canvasWidth), -2.0/(canvasHeight));

    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, 64*1024, gl.DYNAMIC_DRAW);
    gl.vertexAttribIPointer(0, 4, gl.UNSIGNED_INT, false, 0, 0);
    gl.enableVertexAttribArray(0);
    gl.bindAttribLocation(glProgram, 0, "packed_ball_data");

    wasmInstance.exports._start(canvasWidth, canvasHeight);
    wasmStarted = true;

    requestAnimationFrame(doUpdateLoop);
}

function drawBalls2d(structSize, arrayPointer, count) {
    canvasContext2d.clearRect(0, 0, canvasWidth, canvasHeight);
    for (var i = arrayPointer;
             i < arrayPointer+structSize*count;
             i += structSize
    ) {
        var ballX              = wasmMemoryFloat32View[ i    / 4];
        var ballY              = wasmMemoryFloat32View[(i+4) / 4];
        var ballRadiusAndColor = wasmMemoryUint32View [(i+8) / 4];

        canvasContext2d.beginPath();
        canvasContext2d.lineWidth = 2*(ballRadiusAndColor & 255);
        var cssColorString = String.fromCharCode(
            '#'.charCodeAt(0),
            (ballRadiusAndColor>>10&15) + ((ballRadiusAndColor>>10&15) > 9)*7
            + '0'.charCodeAt(0),
            (ballRadiusAndColor>> 8&15) + ((ballRadiusAndColor>> 8&15) > 9)*7
            + '0'.charCodeAt(0),
            (ballRadiusAndColor>>20&15) + ((ballRadiusAndColor>>20&15) > 9)*7
            + '0'.charCodeAt(0),
            (ballRadiusAndColor>>16&15) + ((ballRadiusAndColor>>16&15) > 9)*7
            + '0'.charCodeAt(0),
            (ballRadiusAndColor>>28&15) + ((ballRadiusAndColor>>28&15) > 9)*7
            + '0'.charCodeAt(0),
            (ballRadiusAndColor>>24&15) + ((ballRadiusAndColor>>24&15) > 9)*7
            + '0'.charCodeAt(0));
        canvasContext2d.strokeStyle = cssColorString;
        canvasContext2d.moveTo(ballX, ballY);
        canvasContext2d.lineTo(ballX, ballY);
        canvasContext2d.stroke();
    }
}

function drawBallsWebGL2(structSize, arrayPointer, count) {
    gl.bufferSubData(gl.ARRAY_BUFFER, 0,
                     wasmMemoryUint32View,
                     arrayPointer/4, count*structSize/4);
    gl.drawArrays(gl.POINTS, 0, count);
}

var frameNo = 1;

const wasmImportsEnv = {
    random: function () { return Math.random(); },
    handleString: function (action, pointer) {
        var string = "", charCode = 0;
        for (; charCode = wasmMemoryUint8View[pointer]; ++pointer) {
            string += String.fromCharCode(charCode);
        }
        if (action == PRINT_STRING) {
            console.log(string);
        } else if (action == THROW_ERROR_FROM_STRING) {
            throw new Error(string);
        } else {
            throw new Error(`Unexpected handleString action = ${action}!`);
        }
    },
    drawBalls(structSize, arrayPointer, count) {
        // Call functions in random order
        if (Math.random() < 0.5) {
            var t0 = performance.now();
            drawBalls2d    (structSize, arrayPointer, count);
            var t1 = performance.now();
            drawBallsWebGL2(structSize, arrayPointer, count);
            var t2 = performance.now();

            // For chrome
            console.timeStamp("2d"   , t0, t1, "2d"   , "draw balls");
            console.timeStamp("webgl", t1, t2, "webgl", "draw balls");
            // For firefox
            performance.measure("2d"    , {start: t0, end: t1});
            performance.measure("webgl2", {start: t1, end: t2});
        } else {
            var t0 = performance.now();
            drawBallsWebGL2(structSize, arrayPointer, count);
            var t1 = performance.now();
            drawBalls2d    (structSize, arrayPointer, count);
            var t2 = performance.now();

            // For chrome
            console.timeStamp("webgl", t0, t1, "webgl", "draw balls");
            console.timeStamp("2d"   , t1, t2, "2d"   , "draw balls");
            // For firefox
            performance.measure("webgl2", {start: t0, end: t1});
            performance.measure("2d"    , {start: t1, end: t2});
        }
        ++frameNo;
    }
};

WebAssembly.instantiateStreaming(
    fetch("./balls.wasm"), {env: wasmImportsEnv}
).then(function (result) {
    wasmInstance = result.instance;
    wasmMemoryUint8View   = new Uint8Array  (wasmInstance.exports.memory.buffer);
    wasmMemoryFloat32View = new Float32Array(wasmInstance.exports.memory.buffer);
    wasmMemoryUint32View  = new Uint32Array (wasmInstance.exports.memory.buffer);

    if (!wasmStarted && pageLoaded) { start(); }
    wasmLoaded = true;
});

window.addEventListener("DOMContentLoaded", function handlePageLoad() {
    if (!wasmStarted && wasmLoaded) { start(); }
    pageLoaded = true;
});

