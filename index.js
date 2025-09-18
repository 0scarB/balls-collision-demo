"use strict";

const PRINT_STRING            = 0;
const THROW_ERROR_FROM_STRING = 1;

/** @type {WebAssembly.Instance} **/
var wasmInstance;
/** @type {Uint8Array} **/
var wasmMemoryUint8View;
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
    var canvasElementWebGL = document.getElementById("canvas-webgl2");
    canvasWidth  = canvasElementWebGL.width;
    canvasHeight = canvasElementWebGL.height;
    gl = canvasElementWebGL.getContext("webgl2");
    if (!gl) { throw new Error("Your browser does not support WebGL 2.0!"); }
    var vertexShaderSource =
        "#version 300 es\n"+
        "uniform vec2 ball_xy_mul;\n"+
        "in uvec4 packed_ball_data;\n"+
        "out vec3 ball_color;\n"+
        "void main() {\n"+
        "   gl_Position =\n"+
        "       vec4(uintBitsToFloat(packed_ball_data.x)*ball_xy_mul.x - 1.0,\n"+
        "            uintBitsToFloat(packed_ball_data.y)*ball_xy_mul.y + 1.0,\n"+
        "            0.0, 1.0);\n"+
        "   gl_PointSize = 2.0*float(packed_ball_data[2] & 255u);\n"+
        "   uint color_byte = packed_ball_data[3] & 255u;\n"+
        "   ball_color.r = float(color_byte >> 5)/8.0;\n"+
        "   ball_color.g = float(color_byte >> 3 & 7u)/8.0;\n"+
        "   ball_color.b = float(color_byte & 3u)/4.0;\n"+
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
        "in  vec3 ball_color;\n"+
        "out vec4 frag_color;\n"+
        "void main() {\n"+
        "   vec2 v = gl_PointCoord - vec2(0.5, 0.5);\n"+
        "   frag_color = vec4(ball_color, float(dot(v, v) < 0.25));\n"+
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
    gl.uniform2f(ballXYMulUniform,  2.0/canvasWidth,
                                   -2.0/canvasHeight);

    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, 64*1024, gl.DYNAMIC_DRAW);
    gl.vertexAttribIPointer(0, 4, gl.UNSIGNED_INT, false, 0, 0);
    gl.enableVertexAttribArray(0);
    gl.bindAttribLocation(glProgram, 0, "packed_ball_data");

    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    wasmInstance.exports._start(canvasWidth, canvasHeight);
    wasmStarted = true;

    requestAnimationFrame(doUpdateLoop);
}

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
        gl.bufferSubData(gl.ARRAY_BUFFER, 0,
                         wasmMemoryUint32View,
                         arrayPointer/4, count*structSize/4);
        gl.drawArrays(gl.POINTS, 0, count);
    },
};

WebAssembly.instantiateStreaming(
    fetch("./balls.wasm"), {env: wasmImportsEnv}
).then(function (result) {
    wasmInstance = result.instance;
    wasmMemoryUint8View  = new Uint8Array  (wasmInstance.exports.memory.buffer);
    wasmMemoryUint32View = new Uint32Array (wasmInstance.exports.memory.buffer);

    if (!wasmStarted && pageLoaded) { start(); }
    wasmLoaded = true;
});

window.addEventListener("DOMContentLoaded", function handlePageLoad() {
    if (!wasmStarted && wasmLoaded) { start(); }
    pageLoaded = true;
});

