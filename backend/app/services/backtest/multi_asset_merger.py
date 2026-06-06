import heapq
from typing import Iterator, List
from .data_loader import stream_ticks, Tick
import os

def merge_tick_streams(data_dir: str, symbols: List[str]) -> Iterator[Tick]:
    streams = []
    for sym in symbols:
        for ext in ('.parquet', '.csv'):
            path = os.path.join(data_dir, f"{sym}{ext}")
            if os.path.exists(path):
                streams.append(stream_ticks(path))
                break
    if not streams:
        raise FileNotFoundError(f"No tick data found in {data_dir} for {symbols}")
    yield from heapq.merge(*streams, key=lambda t: t.timestamp_ms)
