import argparse
import json
import math
import sys
import traceback
from pathlib import Path

from training_protocol import TrainingProtocol


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    parser.add_argument('--run-dir', required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text(encoding='utf-8'))
    run_dir = Path(args.run_dir)
    protocol = TrainingProtocol(run_dir, config['job_id'], config['run_id'], 'line_detection')
    protocol.start()
    protocol.emit('worker_ready')
    protocol.emit('started')
    sys.stdout = sys.stderr
    try:
        import cv2
        import numpy as np
        import torch
        from edge_geometry import LineGeometryError, extract_geometry
        from line_model import SIZE, make_model, read_image, tensor_image

        confidence = config.get('confidence', 0.5)
        if isinstance(confidence, bool) or not isinstance(confidence, (int, float)) or not math.isfinite(confidence) or not 0 <= confidence <= 1:
            raise ValueError('置信度必须是 0 到 1 之间的有限数字')
        model = make_model(config['model'])
        model.eval()
        failed_images = 0
        for index, image_path in enumerate(config['sources']):
            if protocol.cancel.is_set():
                protocol.emit('cancelled')
                return
            try:
                image = read_image(image_path)
                height, width = image.shape[:2]
                resized = cv2.resize(image, SIZE, interpolation=cv2.INTER_AREA)
                with torch.inference_mode():
                    probability = model(tensor_image(resized))[-1][0, 0].cpu().numpy()
                mask = cv2.resize(probability, (width, height), interpolation=cv2.INTER_LINEAR) >= confidence
                mask_name = f'{index:06d}_mask.png'
                ok, encoded = cv2.imencode('.png', mask.astype(np.uint8) * 255)
                if not ok:
                    raise RuntimeError('Cannot encode the model edge mask')
                encoded.tofile(str(run_dir / mask_name))

                result = {'imagePath': image_path, 'imageWidth': width, 'imageHeight': height,
                          'mask': mask_name, 'confidence': confidence}
                shapes = []
                try:
                    _, paths = extract_geometry(mask, 20.0, 5.0, 40.0)
                    for path in paths:
                        for segment in path['segments']:
                            points = np.clip(np.array([segment['start'], segment['end']]),
                                             [0, 0], [width - 1, height - 1]).tolist()
                            shapes.append({'shape_type': 'line', 'points': points})
                except LineGeometryError as error:
                    failed_images += 1
                    if not mask.any():
                        result['errorMessage'] = (f'{image_path}: 模型未检出边缘, 最大概率 '
                                                  f'{float(probability.max()):.4f}, 阈值 {confidence:.4f}')
                    else:
                        result['errorMessage'] = f'{image_path}: {error}'
                result['shapes'] = shapes
                result_path = run_dir / f'{index:06d}.json'
                result_path.write_text(json.dumps(result, ensure_ascii=False), encoding='utf-8')
                event = {'index': index, 'result': str(result_path)}
                if 'errorMessage' in result:
                    event['errorMessage'] = result['errorMessage']
                protocol.emit('image', **event)
            except Exception as error:
                raise RuntimeError(f'{image_path}: {error}') from error
        if failed_images:
            raise RuntimeError(f'No valid fitted lines in {failed_images} image(s)')
        protocol.emit('finished')
    except Exception as error:
        traceback.print_exc(file=sys.stderr)
        protocol.emit('error', message=str(error), errorMessage=str(error))
        raise
    finally:
        protocol.done.set()


if __name__ == '__main__':
    main()
