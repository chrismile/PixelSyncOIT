/**
 * Originally, the two defines below were used. They worked fine on the open-source Linux graphics drivers,
 * but caused problem on the closed-source NVIDIA drivers (for whatever reason)...
 */
//#define CONCAT_VERTEX_ATTRIBUTE_NAME(n) vertexAttribute##n
//#define vertexAttribute CONCAT_VERTEX_ATTRIBUTE_NAME(IMPORTANCE_CRITERION_INDEX)

#if IMPORTANCE_CRITERION_INDEX == 0
#define VERTEX_ATTRIBUTE vertexAttribute0
#elif IMPORTANCE_CRITERION_INDEX == 1
#define VERTEX_ATTRIBUTE vertexAttribute1
#elif IMPORTANCE_CRITERION_INDEX == 2
#define VERTEX_ATTRIBUTE vertexAttribute2
#elif IMPORTANCE_CRITERION_INDEX == 3
#define VERTEX_ATTRIBUTE vertexAttribute3
#elif IMPORTANCE_CRITERION_INDEX == 4
#define VERTEX_ATTRIBUTE vertexAttribute4
#elif IMPORTANCE_CRITERION_INDEX == 5
#define VERTEX_ATTRIBUTE vertexAttribute5
#elif IMPORTANCE_CRITERION_INDEX == 6
#define VERTEX_ATTRIBUTE vertexAttribute6
#elif IMPORTANCE_CRITERION_INDEX == 7
#define VERTEX_ATTRIBUTE vertexAttribute7
#elif IMPORTANCE_CRITERION_INDEX == 8
#define VERTEX_ATTRIBUTE vertexAttribute8
#elif IMPORTANCE_CRITERION_INDEX == 9
#define VERTEX_ATTRIBUTE vertexAttribute9
#endif