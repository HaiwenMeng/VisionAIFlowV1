"""Convert an edge probability map into ordered paths and fitted line segments."""

import cv2
import numpy as np


class LineGeometryError(ValueError):
    """The model mask did not yield a usable line segment."""


def thin_mask(mask):
    """Zhang-Suen thinning with simultaneous deletions in each phase."""
    pixels = np.pad((mask != 0).astype(np.uint8), 1)
    stride = pixels.shape[1]
    flat = pixels.ravel()
    offsets = np.array([-stride, 1 - stride, 1, stride + 1, stride, stride - 1, -1, -stride - 1])
    boundary = np.flatnonzero(pixels != cv2.erode(pixels, np.ones((3, 3), dtype=np.uint8)))
    pending = [boundary, boundary]
    next_neighbor = [1, 2, 3, 4, 5, 6, 7, 0]
    while pending[0].size or pending[1].size:
        for phase in (0, 1):
            candidates = pending[phase]
            candidates = candidates[flat[candidates] != 0]
            neighbors = flat[candidates[:, None] + offsets]
            count = neighbors.sum(axis=1, dtype=np.uint8)
            transitions = np.count_nonzero((neighbors == 0) & (neighbors[:, next_neighbor] != 0), axis=1)
            p2, p4, p6, p8 = neighbors[:, ::2].T
            if phase == 0:
                condition = (p2 * p4 * p6 == 0) & (p4 * p6 * p8 == 0)
            else:
                condition = (p2 * p4 * p8 == 0) & (p2 * p6 * p8 == 0)
            removed = candidates[(count >= 2) & (count <= 6) & (transitions == 1) & condition]
            flat[removed] = 0
            # Only neighbors of a deletion can change eligibility for either phase.
            affected = np.unique(removed[:, None] + offsets)
            affected = affected[flat[affected] != 0]
            pending[phase] = affected
            pending[1 - phase] = np.union1d(pending[1 - phase], affected)
    return pixels[1:-1, 1:-1] * 255


def trace_polylines(skeleton, min_length):
    """Trace each graph edge once, splitting at endpoints and junctions."""
    ys, xs = np.nonzero(skeleton)
    points = set(zip(xs.tolist(), ys.tolist()))
    graph = {}
    for x, y in points:
        neighbors = []
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                neighbor = (x + dx, y + dy)
                if (dx == 0 and dy == 0) or neighbor not in points:
                    continue
                # Avoid diagonal shortcuts where an orthogonal path already exists.
                if dx and dy and ((x + dx, y) in points or (x, y + dy) in points):
                    continue
                neighbors.append(neighbor)
        graph[(x, y)] = sorted(neighbors)

    visited = set()
    paths = []

    def edge(a, b):
        return tuple(sorted((a, b)))

    def walk(start, following):
        path = [start]
        previous, current = start, following
        visited.add(edge(previous, current))
        while True:
            path.append(current)
            if len(graph[current]) != 2:
                break
            candidate = graph[current][0] if graph[current][1] == previous else graph[current][1]
            if edge(current, candidate) in visited:
                break
            previous, current = current, candidate
            visited.add(edge(previous, current))
        return path

    def add(path):
        coords = np.asarray(path, dtype=np.float64)
        length = float(np.linalg.norm(np.diff(coords, axis=0), axis=1).sum())
        if length >= min_length:
            paths.append(path)

    for point in sorted(graph):
        if len(graph[point]) == 2:
            continue
        for neighbor in graph[point]:
            if edge(point, neighbor) not in visited:
                add(walk(point, neighbor))
    # Components with no junction or endpoint are closed loops.
    for point in sorted(graph):
        for neighbor in graph[point]:
            if edge(point, neighbor) not in visited:
                add(walk(point, neighbor))
    return paths


def simplify_indices(points, tolerance):
    """Return ordered Ramer-Douglas-Peucker split indices."""
    coords = np.asarray(points, dtype=np.float64)
    keep = {0, len(points) - 1}
    stack = [(0, len(points) - 1)]
    while stack:
        first, last = stack.pop()
        if last <= first + 1:
            continue
        start, end = coords[first], coords[last]
        delta = end - start
        norm = np.linalg.norm(delta)
        between = coords[first + 1:last]
        if norm == 0:
            distances = np.linalg.norm(between - start, axis=1)
        else:
            fraction = np.clip(((between - start) @ delta) / (norm * norm), 0.0, 1.0)
            closest = start + fraction[:, None] * delta
            distances = np.linalg.norm(between - closest, axis=1)
        index = first + 1 + int(np.argmax(distances))
        if distances[index - first - 1] > tolerance:
            keep.add(index)
            stack.extend(((first, index), (index, last)))
    return sorted(keep)


def fit_segments(path, tolerance, min_segment_length):
    coords = np.asarray(path, dtype=np.float32)
    indices = simplify_indices(path, tolerance)
    segments = []
    for first, last in zip(indices, indices[1:]):
        sample = coords[first:last + 1]
        if len(sample) < 2:
            continue
        vx, vy, x0, y0 = cv2.fitLine(sample, cv2.DIST_L2, 0, 0.01, 0.01).flatten().astype(float)
        direction = np.array([vx, vy])
        if np.dot(direction, sample[-1] - sample[0]) < 0:
            direction = -direction
        center = np.array([x0, y0])
        projection = (sample - center) @ direction
        low, high = float(projection.min()), float(projection.max())
        if high - low < min_segment_length:
            continue
        residual = (sample[:, 0] - x0) * (-vy) + (sample[:, 1] - y0) * vx
        segments.append({
            "start": (center + low * direction).tolist(),
            "end": (center + high * direction).tolist(),
            "length": high - low,
            "rms_error": float(np.sqrt(np.mean(residual ** 2))),
            "point_count": len(sample),
        })
    return segments


def extract_geometry(mask, min_length=20.0, tolerance=2.0, min_segment_length=10.0):
    if not np.any(mask):
        raise LineGeometryError("Empty edge mask")
    skeleton = thin_mask(mask)
    paths = trace_polylines(skeleton, min_length)
    if not paths:
        raise LineGeometryError("No valid polylines after tracing")
    result = []
    for path in paths:
        segments = fit_segments(path, tolerance, min_segment_length)
        result.append({"points": [list(point) for point in path], "segments": segments})
    if not any(path["segments"] for path in result):
        raise LineGeometryError("No valid fitted line segments")
    return skeleton, result
