import argparse
import json
import random
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
        from line_model import balanced_loss, make_model, read_sample, score, tensor_image

        random.seed(7)
        np.random.seed(7)
        torch.manual_seed(7)
        dataset = json.loads(Path(config['dataset']).read_text(encoding='utf-8'))
        train = [read_sample(sample) for sample in dataset['train']]
        validation = [read_sample(sample) for sample in dataset['val']]
        if not train or not validation:
            raise ValueError('Training and validation datasets must both contain images')
        model = make_model(config['weights'])
        optimizer = torch.optim.Adam(model.parameters(), lr=float(config['learning_rate']))
        total_epochs = int(config['epochs'])
        if total_epochs < 1:
            raise ValueError('Epoch count must be positive')
        best = -1.0
        for epoch in range(1, total_epochs + 1):
            model.train()
            order = list(range(len(train)))
            random.shuffle(order)
            losses = []
            for step, index in enumerate(order, 1):
                if protocol.cancel.is_set():
                    protocol.emit('cancelled')
                    return
                image, label = train[index]
                if random.random() < 0.5:
                    image, label = cv2.flip(image, 1), cv2.flip(label, 1)
                if random.random() < 0.5:
                    image, label = cv2.flip(image, 0), cv2.flip(label, 0)
                target = torch.from_numpy(label.copy()).unsqueeze(0).unsqueeze(0).cuda()
                optimizer.zero_grad(set_to_none=True)
                loss = sum(balanced_loss(output, target) for output in model(tensor_image(image)))
                loss.backward()
                optimizer.step()
                losses.append(loss.item())
                protocol.emit('batch', epoch=epoch, total_epochs=total_epochs, step=step, total_steps=len(order))
            model.eval()
            metrics = []
            with torch.inference_mode():
                for image, label in validation:
                    if protocol.cancel.is_set():
                        protocol.emit('cancelled')
                        return
                    probability = model(tensor_image(image))[-1][0, 0].cpu().numpy()
                    metrics.append(score(probability, label))
            precision, recall, f1 = np.mean(metrics, axis=0).tolist()
            checkpoint = {'state_dict': model.state_dict(), 'epoch': epoch,
                          'validation': {'precision': precision, 'recall': recall, 'f1': f1}}
            torch.save(checkpoint, run_dir / 'last.pth')
            if f1 > best:
                best = f1
                torch.save(checkpoint, run_dir / 'best.pth')
                protocol.emit('checkpoint', best=str(run_dir / 'best.pth'), last=str(run_dir / 'last.pth'))
            protocol.emit('epoch', epoch=epoch, total_epochs=total_epochs, loss=float(np.mean(losses)),
                          precision=precision, recall=recall, f1=f1)
        if protocol.cancel.is_set():
            protocol.emit('cancelled')
            return
        protocol.emit('finished', best=str(run_dir / 'best.pth'), last=str(run_dir / 'last.pth'))
    except Exception as error:
        traceback.print_exc(file=sys.stderr)
        protocol.emit('error', message=str(error), errorMessage=str(error))
        raise
    finally:
        protocol.done.set()


if __name__ == '__main__':
    main()
