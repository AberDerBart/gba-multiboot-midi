import { useCallback, useState } from "react"

export default function RomUploadContainer() {

  const [error, setError] = useState<string | undefined>();
  const [serial, setSerial] = useState<SerialPort>();

  const [isUploading, setIsUploading] = useState(false);

  const [rom, setRom] = useState<File>();

  const selectDevice = useCallback(async () => {
    setSerial(await navigator.serial.requestPort());
  }, [])

  const upload = useCallback(async () => {
    if (!serial || !rom) {
      return;
    }

    setError(undefined);

    setIsUploading(true);

    await serial.open({ baudRate: 115200 });
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
      await rom.stream().pipeTo(serial.writable)
    } catch (e) {
      console.error(e)
      setError(JSON.stringify(e));
    } finally {
      await serial.close();
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
  </div>
}