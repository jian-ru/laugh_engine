# Laugh Engine

A Vulkan implementation of real-time PBR renderer.

---

### Results

* Video demo available on [my homepage](http://jian-ru.github.io/)

| Paper Mill | Factory |
| --- | --- |
| ![](docs/paper_mill.png) | ![](docs/factory.png) |

| Mon Valley | Canyon |
| --- | --- |
| ![](docs/mon_valley.png) | ![](docs/canyon.png) |

### glTF Support

* glTF, also called the GL Transmission Format, it is a runtime asset dilivery format for GL-based applications. It supports common functionalities of 3D applications such as geometry, texture, skin, and animation. It also allows extension, for example, I used the FRAUNHOFER_materials_pbr extension to load PBR textures.
* It aims to form a standard/common format for runtime asset dilivery/exchange. In my opinion, it has better documentation than FBX and possess some abilities that some other file formats don't (e.g. scene hierarchy, skin, animation). I believe, if it gets popular, it will be a great convenience to both application developers and artists because less effort will be required to perform application-specific conversion on file formats.
* In order to show my support, I added glTF support to Laugh Egine. It is now able to load geometry and material data from glTF files.
* Below are some screen shots of 3D models loaded from glTF files. Thanks to Sketchfab for providing these glTF assets.

| Microphone | Centurion |
| --- | --- |
| ![](docs/gltf_demo001.png) | ![](docs/gltf_demo000.png) |

### Overview

| ![](docs/how_it_work0.png) | ![](docs/how_it_work1.png) |
| --- | --- |
| ![](docs/how_it_work3.png) | ![](docs/how_it_work2.png) |
| ![](docs/how_it_work4.png) |

### Performance Analysis

* Precomputation performance
  * It is not real-time but fast enough to be used for interactive editting
  * The increase of execution time from 128x128 to 256x256 is very small. Probably the GPU is not saturated at that time.
  
  | Envrionment Prefiltering | BRDF LUT Baking |
  | --- | --- |
  | ![](docs/perf_env.png) | ![](docs/brdf_perf.png) |

* PBR vs. Blinn-Phong
  * PBR using IBL only
  * Blinn-Phong was tested using two point light sources
  * Framebuffer resolution was 1920x1080 with no AA
  * Bloom was on
  * The scene with a Cerberus pistol in it was used for benchmark
  * Basically no addtional cost by using PBR over Blinn-Phong but we get much better quality
  
  ![](docs/pbr_vs_blinnphong.png)
  
* Anti-Aliasing
  * Increases visual quality (smoother edges and more accurate shading)
  * Decreases performance. For 2xAA, 4xAA, and 8xAA, frame time is increased by 31%, 70%, and 173%, respectively when compared to no AA
  * I did custom resolve in a fragment shader because Laugh Engine is deferred. Right now, I am doing lighting computation on each sample so it is basically SSAA not MSAA. I tried to perform computation only once per material type but the performance was much worse because the code that figures out distinct material types is expensive causes a lot of branching (frame time goes up to 8ms for 4xAA). So I will stick to the current scheme until I find a better way.
  
  | 1xAA | 2xAA |
  | --- | --- |
  | ![](docs/1xaa.png) | ![](docs/2xaa.png) |
  
  | 4xAA | 8xAA |
  | --- | --- |
  | ![](docs/4xaa.png) | ![](docs/8xaa.png) |

### Build Instruction

* Install LunarG Vulkan SDK
* If you have Visual Studio 2015, the solution should build out of the box
* To run the program, you will need to copy the .dlls to executable path or system paths
* Builds on other platforms are not supported yet

---

### Third-Party Credits

#### References:
* [Vulkan Tutorial by Alexander Overvoorde](https://vulkan-tutorial.com)
* [Vulkan Samples by Sascha Willems](https://github.com/SaschaWillems/Vulkan)
* [PBR Course Note](http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
* [IBLBaker](https://github.com/derkreature/IBLBaker)

#### Libraries
* [GLFW](http://www.glfw.org/)
* [GLM](http://glm.g-truc.net/0.9.8/index.html)
* [GLI](http://gli.g-truc.net/0.8.2/index.html)
* [Assimp](http://www.assimp.org/)

#### Assets
* [Mysterious Island Centurion by levikingvolant](https://sketchfab.com/levikingvolant)
* [Microphone GXL 066 Bafhcteks by Gistold](https://sketchfab.com/gistold)
* [Cerberus by Andrew Maximov](http://artisaverb.info/Cerberus.html)
