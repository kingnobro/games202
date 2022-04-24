# games202

### hw0

- 模型呈现灰色的问题：需要在 `PhongMaterial.js` 文件中检查每个字符串是否包含额外的空格

- 模型有时显示有时不显示的问题：在 `loadOBJ.js` 中，`materials.preload();` 是一个异步的方法，该方法会去加载模型的 mtl 文件，image 文件的路径也在其中。然而在这个方法之后，紧接着就去 load object 并且使用了 mtl 文件中的 image，但由于前一步是异步的，材质还没有加载完成，导致了模型不显示的问题。解决方法是，在 `index.html` 中的 21 行加入如下代码，预加载材质图片

  ```javascript
  <link rel="preload" href="/assets/mary/MC003_Kozakura_Mari.png" as="image" type="image/png" crossorigin/>
  ```

[![L46qxJ.gif](https://s1.ax1x.com/2022/04/24/L46qxJ.gif)](https://imgtu.com/i/L46qxJ)

