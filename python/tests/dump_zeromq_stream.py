import json
from pathlib import Path
from typing import Annotated

import typer
import zmq

HERE = Path(__file__).parent


def main(
    url: Annotated[
        str, typer.Argument(help="URL of ZeroMQ server")
    ] = "127.0.0.1:10008",
    output: Annotated[Path, typer.Option(help="Output directory")] = HERE / "input",
):
    """Dump messages from a ZeroMQ PUB stream."""
    if not url.startswith("http://"):
        url = f"tcp://{url}"

    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect(url)
    socket.setsockopt_string(zmq.SUBSCRIBE, "")

    print(f"Connected to {url}")

    idx = 0
    while True:
        print("Waiting for message...")
        message = socket.recv()
        # Check if message is json or raw bytes
        try:
            j = json.loads(message.decode())
            data = json.dumps(j, indent=4)
            ext = ".json"
            mode = "w"
        except (json.decoder.JSONDecodeError, UnicodeDecodeError):
            data = message
            ext = ""
            mode = "wb"

        with (output / f"{idx}{ext}").open(mode=mode) as f:
            f.write(data)

        print(f"{idx}{ext}")

        idx += 1


if __name__ == "__main__":
    typer.run(main)