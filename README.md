# icp-analysis
Analysis of Iterative Closest Point

1 Abstract
Our aim in this project is to analyze the Iterative Closest Point (ICP) algorithm by
comparing and contrasting its different variants.
ICP is an algorithm employed to minimize the difference between two point clouds.
It is often used to reconstruct 2D or 3D surfaces from different scans, to localize robots
and achieve optimal path planning , to register medical scans etc. ICP has several steps
and each step may be implemented in various ways which give rise to a multitude of ICP
variants. These steps are:
1. Selecting sets of points in the source and target meshes.
2. Matching selected points from the source mesh to the target mesh.
3. Weighting the matched points.
4. Rejecting the outliers.
5. Selecting an Error metric
6. Selecting an Optimizing technique for minimizing the error metric for the corresponding pairs from the source and target meshes.
In this project, we will plug in two or more different implementations of stage 2, for
example closest point vs projective ICP (Blais et al. [1]) and stage 6, linearized ICP vs
Levenberg-Marquardt ICP ([2], [3]) or Guass-Newton ICP ([4]) and analyze their effect
on the algorithm’s convergence speed and quality of output.
In addition, we will also contrast heirarchical ([5]) and non-heirarchical ICP in a similar
manner as the above.
2 Requirements
We will not require any special hardware
3 Team
Our team consists of:
• Neha Das
• Sumit Dugar
