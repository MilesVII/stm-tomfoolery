import { readdir } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

import sharp from "sharp";

main();

async function getPngFiles(dirPath: string): Promise<string[]> {
	const entries = await readdir(dirPath, { withFileTypes: true });
	const pngFiles = entries
		.filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith(".png"))
		.map((entry) => join(dirPath, entry.name));

	return pngFiles;
}

async function getPixelData(filePath: string) {
	const image = sharp(filePath);
	const metadata = await image.metadata();
	
	const { data, info } = await image
		.raw()
		.toBuffer({ resolveWithObject: true });

	return {
		path: filePath,
		width: info.width,
		height: info.height,
		channels: info.channels,
		pixels: data,
		size: data.length / info.channels
	};
}

async function main() {
	const directory = "./frames";
	const pngFiles = await getPngFiles(directory);
	let ix = 0;
	const bytes: number[] = [];
	for (const f of pngFiles) {
		++ix;
		const h = 32;
		const w = 48;
		const fix = parseInt(f.split("frame-")[1].split(".png")[0], 10);
		const data = await getPixelData(f);
		if (data.width !== 48 || data.height !== 32 || data.channels !== 3 || fix !== ix) {
			console.log(f, "faulty");
			return;
		}
		const pixels: boolean[] = [];
		for (let i = 0; i < data.size; ++i) {
			pixels.push(data.pixels[i * data.channels] > 0);
		}

		if (ix > 420) break;
		for (let page = 0; page < w / 8; ++page)
		for (let row = 0; row < h; ++row) {
			
			const offset = page * 8 + (h - row - 1) * w;
			let bits = 0;
			for (let b = 0; b < 8; ++b) {
				if (pixels[offset + b])
					bits |= 1 << b;
			}
			bytes.push(bits);
		}
	}
	await Bun.write(`${bytes.length}.txt`, bytes.map(b => "0x" + b.toString(16)).join(","));
	return;
}
