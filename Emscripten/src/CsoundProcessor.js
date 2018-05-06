'use strict';

const WAM = AudioWorkletGlobalScope.WAM;

WAM["ENVIRONMENT"] = "WEB";
WAM["print"] = (t) => console.log(t);
WAM["printErr"] = (t) => console.log(t);


// INITIALIAZE WASM
AudioWorkletGlobalScope.libcsound(WAM);

// SETUP FS

let FS = WAM["FS"];



// Get cwrap-ed functions
const Csound = {

    new: WAM.cwrap('CsoundObj_new', ['number'], null),
    compileCSD: WAM.cwrap('CsoundObj_compileCSD', ['number'], ['number', 'string']),
    evaluateCode: WAM.cwrap('CsoundObj_evaluateCode', ['number'], ['number', 'string']),
    readScore: WAM.cwrap('CsoundObj_readScore', ['number'], ['number', 'string']),

    reset: WAM.cwrap('CsoundObj_reset', null, ['number']),
    getOutputBuffer: WAM.cwrap('CsoundObj_getOutputBuffer', ['number'], ['number']),
    getInputBuffer: WAM.cwrap('CsoundObj_getInputBuffer', ['number'], ['number']),
    getControlChannel: WAM.cwrap('CsoundObj_getControlChannel', ['number'], ['number', 'string']),
    setControlChannel: WAM.cwrap('CsoundObj_setControlChannel', null, ['number', 'string', 'number']),
    setStringChannel: WAM.cwrap('CsoundObj_setStringChannel', null, ['number', 'string', 'string']),
    getKsmps: WAM.cwrap('CsoundObj_getKsmps', ['number'], ['number']),
    performKsmps: WAM.cwrap('CsoundObj_performKsmps', ['number'], ['number']),

    render: WAM.cwrap('CsoundObj_render', null, ['number']),
    getInputChannelCount: WAM.cwrap('CsoundObj_getInputChannelCount', ['number'], ['number']),
    getOutputChannelCount: WAM.cwrap('CsoundObj_getOutputChannelCount', ['number'], ['number']),
    getTableLength: WAM.cwrap('CsoundObj_getTableLength', ['number'], ['number', 'number']),
    getTable: WAM.cwrap('CsoundObj_getTable', ['number'], ['number', 'number']),
    getZerodBFS: WAM.cwrap('CsoundObj_getZerodBFS', ['number'], ['number']),
    compileOrc: WAM.cwrap('CsoundObj_compileOrc', 'number', ['number', 'string']),
    setOption: WAM.cwrap('CsoundObj_setOption', null, ['number', 'string']),
    prepareRT: WAM.cwrap('CsoundObj_prepareRT', null, ['number']),
    getScoreTime: WAM.cwrap('CsoundObj_getScoreTime', null, ['number']),
    setTable: WAM.cwrap('CsoundObj_setTable', null, ['number', 'number', 'number', 'number']),
    pushMidiMessage: WAM.cwrap('CsoundObj_pushMidiMessage', null, ['number', 'number', 'number', 'number']),
    setMidiCallbacks: WAM.cwrap('CsoundObj_setMidiCallbacks', null, ['number']),

}

class CsoundProcessor extends AudioWorkletProcessor {

    static get parameterDescriptors() {
        return [];
    }

    constructor(options) {
        super(options);
        let p = this.port;
        WAM["print"] = (t) => {
            p.postMessage(["log", t]);
        };
        WAM["printErr"] = (t) => {
            p.postMessage(["log", t]);
        };

        let csObj = Csound.new();
        this.csObj = csObj;
        // engine status
        this.status = 0;
        this.running = false;
        this.started = false;
        this.sampleRate = options.sampleRate;  

        Csound.setMidiCallbacks(csObj);
        Csound.setOption(csObj, "-odac");
        Csound.setOption(csObj, "-iadc");
        Csound.setOption(csObj, "-M0");
        Csound.setOption(csObj, "-+rtaudio=null");
        Csound.setOption(csObj, "-+rtmidi=null");
        Csound.setOption(csObj, "--sample-rate="+this.sampleRate);  
        Csound.prepareRT(csObj);
        this.nchnls = options.numberOfOutputs;
        this.nchnls_i = options.numberOfInputs;
        Csound.setOption(csObj, "--nchnls=" + this.nchnls);
        Csound.setOption(csObj, "--nchnls_i=" + this.nchnls_i); 

        this.port.onmessage = this.handleMessage.bind(this);
        this.port.start();
    }


    process(inputs, outputs, parameters) {
        if (this.csoundOutputBuffer == null ||
            this.running == false) {
            return true;
        }

        let input = inputs[0];
        let output = outputs[0];

        let bufferLen = output[0].length;

        let csOut = this.csoundOutputBuffer;
        let csIn = this.csoundInputBuffer;
        let ksmps = this.ksmps;
        let zerodBFS = this.zerodBFS;

        let cnt = this.cnt;
        let nchnls = this.nchnls;
        let nchnls_i = this.nchnls_i;
        let status = this.status;

        for (let i = 0; i < bufferLen; i++, cnt++) {
            if(cnt == ksmps && status == 0) {
                // if we need more samples from Csound
                status = Csound.performKsmps(this.csObj);
                cnt = 0;
            }

            for (let channel = 0; channel < input.length; channel++) {
                let inputChannel = input[channel];
                csIn[cnt*nchnls_i + channel] = inputChannel[i] * zerodBFS;
            }
            for (let channel = 0; channel < output.length; channel++) {
                let outputChannel = output[channel];
                if(status == 0)
                    outputChannel[i] = csOut[cnt*nchnls + channel] / zerodBFS;
                else
                    outputChannel[i] = 0;
            } 
        }

        this.cnt = cnt;
        this.status = status;

        return true;
    }

    start() {
        if(this.started == false) {
            let csObj = this.csObj;
            let ksmps = Csound.getKsmps(csObj);
            this.ksmps = ksmps;
            this.cnt = ksmps;
            
            let outputPointer = Csound.getOutputBuffer(csObj);
            this.csoundOutputBuffer = new Float32Array(WAM.HEAP8.buffer, outputPointer, ksmps * this.nchnls);
            let inputPointer = Csound.getInputBuffer(csObj);
            this.csoundInputBuffer = new Float32Array(WAM.HEAP8.buffer, inputPointer, ksmps * this.nchnls_i);
            this.zerodBFS = Csound.getZerodBFS(csObj);
            this.started = true;
        }
        this.running = true;
    }

    compileOrc(orcString) {
        Csound.compileOrc(this.csObj, orcString);
    }

    handleMessage(event) {
        let data = event.data;
        let p = this.port;

        switch (data[0]) {
        case "compileCSD":
            this.status = Csound.compileCSD(this.csObj, data[1]);
            break;
        case "compileOrc":
            Csound.compileOrc(this.csObj, data[1]);
            break;
        case "evalCode":
            Csound.evaluateCode(this.csObj, data[1]);
            break;
        case "readScore":
            Csound.readScore(this.csObj, data[1]);
            break;
        case "setControlChannel":
            Csound.setControlChannel(this.csObj,
                                     data[1], data[2]);
            break;
        case "setStringChannel":
            Csound.setStringChannel(this.csObj,
                                    data[1], data[2]);
            break;
        case "start":
            this.start();
            break;
        case "stop":
            this.running = false;
            break;
        case "play":
            this.start();
            break;
        case "setOption":
            Csound.setOption(this.csObj, data[1]);
            break;
        case "reset":
            let csObj = this.csObj;
            Csound.setMidiCallbacks(csObj);
            Csound.setOption(csObj, "-odac");
            Csound.setOption(csObj, "-iadc");
            Csound.setOption(csObj, "-M0");
            Csound.setOption(csObj, "-+rtaudio=null");
            Csound.setOption(csObj, "-+rtmidi=null");
            Csound.setOption(csObj, "--sample-rate="+this.sampleRate);  
            Csound.prepareRT(csObj);
            this.nchnls = options.numberOfOutputs;
            this.nchnls_i = options.numberOfInputs;
            Csound.setOption(csObj, "--nchnls=" + this.nchnls);
            Csound.setOption(csObj, "--nchnls_i=" + this.nchnls_i);
            this.csoundOutputBuffer = null; 
            this.ksmps = null; 
            this.zerodBFS = null; 
            break;
        case "writeToFS":
            let name = data[1];
            let blobData = data[2];
            let buf = new Uint8Array(blobData)
            let stream = FS.open(name, 'w+');
            FS.write(stream, buf, 0, buf.length, 0);
            FS.close(stream);

            break;
        case "midiMessage":
            let byte1 = data[1];
            let byte2 = data[2];
            let byte3 = data[3];
            Csound.pushMidiMessage(this.csObj, byte1, byte2, byte3);
            break;
        case "getChannel":
            let channel = data[1];
            let value = Csound.getControlChannel(csObj, channel);
            p.postMessage(["control", channel, value]);
        default:
            console.log('[CsoundAudioProcessor] Invalid Message: "' + event.data);
        }
    }
}

registerProcessor('Csound', CsoundProcessor);
