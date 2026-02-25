# Minimal usage example
import numpy as np
from feature_extraction import Event, FileBehaviorFeatureExtractor

events = [
    Event(operation="Create", path=r"C:\Users\Alice\Documents\report.docx", time=0.10),
    Event(operation="Write", path=r"C:\Users\Alice\Documents\report.docx", time=0.20),
    Event(
        operation="Rename",
        path=r"C:\Users\Alice\Documents\report.docx",
        new_path=r"C:\Users\Alice\Documents\report.docx.locked",
        time=0.30,
    ),
]

extractor = FileBehaviorFeatureExtractor(window_seconds=10.0)
x = extractor.extract(events)  # np.ndarray, shape (51,), dtype float32
names = extractor.feature_names()
print(x.shape, x.dtype)
print(names[0], names[-1])
