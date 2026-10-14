"""Summarize benchmark windows, including host Present submission intervals."""
import argparse
import csv
import importlib.util
import json
from pathlib import Path


def summarize_run(folder):
    runtime_summary = Path(__file__).resolve().parents[1] / 'xboxrecomp/tools/bench/frames.py'
    spec = importlib.util.spec_from_file_location('frame_summary', runtime_summary)
    frames = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(frames)
    windows = json.loads((folder / 'windows.json').read_text(encoding='utf-8-sig'))
    with (folder / 'frames.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    results = []
    for window in windows:
        result = {'name': window['name']}
        duration = window['end'] - window['start']
        result['process_cpu_percent'] = window['cpu_ms'] / duration / window['logical_processors'] * 100
        for metric, timestamp in [('frame_ms', 'qpc_ms'), ('present_ms', 'present_qpc_ms')]:
            values = [float(row[metric]) for row in rows if row.get(metric) and row.get(timestamp)
                      and float(row[metric]) > 0
                      and float(row[timestamp]) - float(row[metric]) >= window['start']
                      and float(row[timestamp]) <= window['end']]
            if len(values) < 10:
                raise ValueError(f"{window['name']}: insufficient {metric} samples ({len(values)})")
            result[metric] = frames.summarize(values)
            print(f"{window['name']} {metric}: {frames.fmt(result[metric])}")
        print(f"{window['name']} process CPU: {result['process_cpu_percent']:.2f}%")
        results.append(result)
    (folder / 'summary.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    summarize_run(parser.parse_args().folder)
