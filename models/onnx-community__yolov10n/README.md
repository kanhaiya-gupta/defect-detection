---
library_name: transformers.js
pipeline_tag: object-detection
license: agpl-3.0
---

# YOLOv10: Real-Time End-to-End Object Detection

ONNX weights for https://github.com/THU-MIG/yolov10.

Latency-accuracy trade-offs             |  Size-accuracy trade-offs
:-------------------------:|:-------------------------:
![latency-accuracy trade-offs](https://cdn-uploads.huggingface.co/production/uploads/61b253b7ac5ecaae3d1efe0c/cXru_kY_pRt4n4mHERnFp.png)  |  ![size-accuracy trade-offs](https://cdn-uploads.huggingface.co/production/uploads/61b253b7ac5ecaae3d1efe0c/8apBp9fEZW2gHVdwBN-nC.png)

## Usage (Transformers.js)

If you haven't already, you can install the [Transformers.js](https://huggingface.co/docs/transformers.js) JavaScript library from [NPM](https://www.npmjs.com/package/@xenova/transformers) using:
```bash
npm i @xenova/transformers
```

**Example:** Perform object-detection.
```js
import { AutoModel, AutoProcessor, RawImage } from '@xenova/transformers';

// Load model
const model = await AutoModel.from_pretrained('onnx-community/yolov10n', {
    // quantized: false,    // (Optional) Use unquantized version.
})

// Load processor
const processor = await AutoProcessor.from_pretrained('onnx-community/yolov10n');

// Read image and run processor
const url = 'https://huggingface.co/datasets/Xenova/transformers.js-docs/resolve/main/city-streets.jpg';
const image = await RawImage.read(url);
const { pixel_values, reshaped_input_sizes } = await processor(image);

// Run object detection
const { output0 } = await model({ images: pixel_values });
const predictions = output0.tolist()[0];

const threshold = 0.5;
const [newHeight, newWidth] = reshaped_input_sizes[0]; // Reshaped height and width
const [xs, ys] = [image.width / newWidth, image.height / newHeight]; // x and y resize scales
for (const [xmin, ymin, xmax, ymax, score, id] of predictions) {
    if (score < threshold) continue;

    // Convert to original image coordinates
    const bbox = [xmin * xs, ymin * ys, xmax * xs, ymax * ys].map(x => x.toFixed(2)).join(', ');
    console.log(`Found "${model.config.id2label[id]}" at [${bbox}] with score ${score.toFixed(2)}.`);
}
// Found "car" at [559.30, 472.72, 799.58, 598.15] with score 0.95.
// Found "car" at [221.91, 422.56, 498.09, 521.85] with score 0.94.
// Found "bicycle" at [1.59, 646.99, 137.72, 730.35] with score 0.92.
// Found "bicycle" at [561.25, 593.65, 695.01, 671.73] with score 0.91.
// Found "person" at [687.74, 324.93, 739.70, 415.04] with score 0.89.
// ...
```