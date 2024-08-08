This repo has some C++ PCG nodes for UE5.4

Create Tagged Spline- Mostly a copy paste of the native create spline node, but will parse tags and add them to the actor (in addition of default pcg added tags)

Create Splines mesh- will spawn spline mesh components along the spline (PCG has this native in 5.5, mine is not a back-port and is probably less stable/good).

ExportPointstoPCGAsset- allows you to ouput point data to a static PCGDataAsset directly from the graph. (Will be replaced by a native version in 5.5)

Export to Foliage-Can output meshes to in instanced foliage, needs to be fed a string attribute of the path to the Static Mesh Foliage Type asset that is used in UE's foliage mode. (Doesn't render meshes immediately, a change needs to be triggered with foliage mode open, still investigating)
Be careful to only run your graph once with this as re-running it will spawn duplicates, can be paired with saving down to a PCG Asset for easy re-use. 
