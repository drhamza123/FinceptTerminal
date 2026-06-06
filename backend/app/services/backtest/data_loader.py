from typing import Iterator, NamedTuple
import os
import csv

class Tick(NamedTuple):
    timestamp_ms: int
    symbol: str
    bid: float
    ask: float
    volume: float

def stream_ticks_from_csv(file_path: str) -> Iterator[Tick]:
    with open(file_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            yield Tick(
                int(row['timestamp_ms']),
                row['symbol'],
                float(row['bid']),
                float(row['ask']),
                float(row.get('volume', 0))
            )

def stream_ticks_from_parquet(file_path: str, chunk_size: int = 100_000) -> Iterator[Tick]:
    import pandas as pd
    for chunk in pd.read_parquet(file_path, chunksize=chunk_size):
        for _, row in chunk.iterrows():
            yield Tick(
                int(row['timestamp_ms']),
                row['symbol'],
                float(row['bid']),
                float(row['ask']),
                float(row.get('volume', 0))
            )

def stream_ticks(file_path: str, chunk_size: int = 100_000) -> Iterator[Tick]:
    if file_path.endswith('.csv'):
        yield from stream_ticks_from_csv(file_path)
    elif file_path.endswith('.parquet'):
        try:
            yield from stream_ticks_from_parquet(file_path, chunk_size)
        except ImportError:
            raise ImportError("pyarrow or fastparquet required for .parquet files")
    else:
        raise ValueError(f"Unsupported file type: {file_path}")
