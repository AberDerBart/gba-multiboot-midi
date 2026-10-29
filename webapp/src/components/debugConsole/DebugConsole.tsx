import { useCallback, useRef, useState } from "react";

import styles from './DebugConsole.module.css';

interface Props {
  sendCmd: (cmd: string) => Promise<void>;
  output: string;
}

export default function DebugConsole({ output, sendCmd }: Props) {
  const [prompt, setPrompt] = useState("");
  const [isSending, setIsSending] = useState(false);

  const promptRef = useRef<HTMLInputElement>(null);

  const send = useCallback(async () => {
    const msg = prompt;
    setPrompt("");
    setIsSending(true);
    await sendCmd(msg);
    setIsSending(false);
    promptRef.current?.focus();
  }, [prompt])

  const handleEnter = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      send();
    }
  }, []);

  return <form
    onSubmit={(e) => {
      e.preventDefault();
      send();
    }}
  >
    <textarea
      className={styles.Output}
      value={output}
      readOnly
    />
    <div className={styles.InputLine}>
      <input
        className={styles.CmdInput}
        ref={promptRef}
        value={prompt}
        onChange={(e) => setPrompt(e.target.value)}
        onKeyDown={handleEnter}
      />
      <button type="submit"
        className={styles.Button}
        disabled={isSending}
      >
        Send
      </button>
    </div>
  </form>
}