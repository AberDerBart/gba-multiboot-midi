import { useCallback, useState } from "react"
import DebugConsole from "../components/debugConsole/DebugConsole";

export default function RomUploadContainer() {

  const [error, setError] = useState<string | undefined>();
  const [serial, setSerial] = useState<SerialPort>();

  const [isUploading, setIsUploading] = useState(false);

  const [rom, setRom] = useState<File>();
  const [output, setOutput] = useState("");

  const sendCmd = useCallback(async (cmd: string) => {
    if (!serial?.writable) {
      return;
    }
    const writer = serial.writable.getWriter();
    await writer.write(new TextEncoder().encode(`${cmd}\n`));
    writer.releaseLock();
  }, []);

  const selectDevice = useCallback(async () => {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    await port.setSignals({ dataTerminalReady: true });
    setSerial(port);

    // read forever and append to the textarea
    const reader = port.readable!.getReader();
    const decoder = new TextDecoder();
    while (true) {
      console.log("waiting for data");
      const { value, done } = await reader.read();
      console.log("breaking");
      if (done) break;
      console.log("read", value);
      setOutput((o) => o + decoder.decode(value));
    }
  }, [])

  const upload = useCallback(async () => {
    if (!serial || !rom) {
      return;
    }

    setError(undefined);

    setIsUploading(true);

    try {
      if (!serial.writable) {
        console.error("serial is not writable");
        return;
      }
      const writer = serial.writable.getWriter();

      const prompt = new TextEncoder().encode("\nupload\n");
      await writer.write(prompt);

      // send size
      const sizeBuffer = new ArrayBuffer(4);
      new DataView(sizeBuffer).setUint32(0, rom.size, true);
      await writer.write(new Uint8Array(sizeBuffer));
      await writer.releaseLock();

      // send data
      await rom.stream().pipeTo(serial.writable, { preventClose: true })
    } catch (e) {
      console.error(e)
      setError(JSON.stringify(e));
    } finally {
      setIsUploading(false);
    }
  }, [serial, rom]);

  return <div>
    <div>{error}</div>
    <button type="button" onClick={selectDevice}>Select device</button>
    <input
      disabled={!serial}
      type="file"
      onChange={(e) => {
        if (!e.target.files?.length) {
          setError("Select a file!");
          return;
        }
        if (e.target.files.length > 1) {
          setError("Select a single file!");
          return;
        }
        setRom(e.target.files[0]);
      }}
    />
    <button onClick={upload} disabled={!rom || !serial || isUploading}>Upload</button>
    {isUploading && <div>Uploading…</div>}
    <DebugConsole sendCmd={sendCmd} output={output} />
  </div>
}