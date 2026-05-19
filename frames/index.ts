import { readdir } from "node:fs/promises";
import { join } from "node:path";

import sharp from "sharp";

main();

async function main() {
	(await getPngFiles("./icons")).forEach(async (f, fix) => {
		const h = 16;
		const w = 16;
		const ix = (x: number, y: number) => x + y * w;
		const data = await getPixelData(f);
		if (data.width !== w || data.height !== h) {
			console.log(f, "faulty");
			return;
		}
		const pixels: boolean[] = [];
		for (let i = 0; i < data.size; ++i) {
			pixels.push(data.pixels[i * data.channels]! === 0);
		}

		const bytes: number[] = [];
		for (let row = 0; row < h; ++row)
		for (let page = 0; page < w / 8; ++page) {
			// const offset = row + (h - page * 8 - 1) * w;
			let bits = 0;
			for (let b = 0; b < 8; ++b) {
				if (pixels[ix(row, b + page * 8)])
					bits |= 1 << b;
			}
			bytes.push(bits);
		}
		Bun.write(`${fix}.txt`, bytes.map(b => `0x${b.toString(16)}`).join(", ").toUpperCase());
	})
}

async function getPngFiles(dirPath: string): Promise<string[]> {
	const entries = await readdir(dirPath, { withFileTypes: true });
	const pngFiles = entries
		.filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith(".png"))
		.map((entry) => join(dirPath, entry.name));

	return pngFiles;
}

async function getPixelData(filePath: string) {
	const image = sharp(filePath);
	// const metadata = await image.metadata();
	
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