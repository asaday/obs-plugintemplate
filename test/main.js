const net = require("net");
const fs = require("fs");
const WaveFile = require("wavefile");

const sleep = (msec) => new Promise((resolve) => setTimeout(resolve, msec));

const pipe = "\\\\.\\pipe\\comment_audio"; // equivalent to Unix /tmp/my-pipe.sock

let client = null;

async function openClient(path) {
  return new Promise((resolve, reject) => {
    if (client) {
      resolve(client);
      return;
    }
    client = net.connect(path, () => {
      console.log("connect");
      resolve(client);
    });
  });
}

function makeSendData(wave) {
  const w = new WaveFile.WaveFile(wave);
  const h = new Buffer.alloc(16);
  w.toBitDepth(16); // force fix

  console.log(w.fmt);
  console.log(w.data.samples.length);
  h.writeUInt16LE(0x2525, 0);
  h.writeUInt16LE(16, 2);
  h.writeUint32LE(w.data.samples.length, 4);
  h.writeUInt16LE(w.fmt.sampleRate, 8);
  h.writeUInt16LE(w.fmt.bitsPerSample, 10);
  h.writeUint32LE(0, 12);

  console.log(h);
  const r = Buffer.concat([h, w.data.samples]);
  return r;
}

async function foo() {
  const d = fs.readFileSync("./n-voice-talk-10.wav");
  const r = makeSendData(d);

  await openClient(pipe);
 await client.write(r);
sleep(2000);
await client.write(r);
console.log('end')
//    process.exit(0)
}

foo().then();

/*

format
0 | magic 2 | 0x25225
2 | header length 2 | 16
4 | data length 4 | ....
8 | sample rate 2 | 16000
10 | bit rate 2 | 16 
12 | nop 4 | 0000 0000
data ...


*/
