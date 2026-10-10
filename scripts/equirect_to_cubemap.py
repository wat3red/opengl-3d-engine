import os
import math
import numpy as np
from PIL import Image

def equirect_to_cubemap(input_path, output_dir, face_size=1024):
    os.makedirs(output_dir, exist_ok=True)
    img = Image.open(input_path).convert("RGB")
    src = np.array(img)
    src_h, src_w, _ = src.shape

    # Generate grid [-1, 1]
    linspace = np.linspace(-1.0, 1.0, face_size, dtype=np.float32)
    x_grid, y_grid = np.meshgrid(linspace, linspace)

    faces = {
        "posx": (np.ones_like(x_grid), -y_grid, -x_grid),
        "negx": (-np.ones_like(x_grid), -y_grid, x_grid),
        "posy": (x_grid, np.ones_like(y_grid), y_grid),
        "negy": (x_grid, -np.ones_like(y_grid), -y_grid),
        "posz": (x_grid, -y_grid, np.ones_like(x_grid)),
        "negz": (-x_grid, -y_grid, -np.ones_like(x_grid)),
    }

    for face_name, (vx, vy, vz) in faces.items():
        # Normalize vectors
        norm = np.sqrt(vx * vx + vy * vy + vz * vz)
        vx /= norm
        vy /= norm
        vz /= norm

        # Map to equirectangular coordinates
        theta = np.arctan2(vx, -vz)
        phi = np.arcsin(np.clip(vy, -1.0, 1.0))

        u = (theta / np.pi + 1.0) * 0.5
        v = 0.5 - (phi / np.pi)

        px = np.clip((u * (src_w - 1)).astype(np.int32), 0, src_w - 1)
        py = np.clip((v * (src_h - 1)).astype(np.int32), 0, src_h - 1)

        face_img_data = src[py, px]
        face_img = Image.fromarray(face_img_data)
        out_file = os.path.join(output_dir, f"{face_name}.jpg")
        face_img.save(out_file, quality=95)
        print(f"Generated {out_file}")

if __name__ == "__main__":
    equirect_to_cubemap(
        "res/models/skybox/textures/sky_water_landscape.jpg",
        "res/textures/skybox_basic_sky",
        face_size=1024
    )
    print("Cubemap conversion complete!")
