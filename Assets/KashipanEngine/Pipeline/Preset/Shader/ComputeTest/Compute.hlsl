// Minimal compute shader for load/compile testing
// No resources; does nothing but should compile and create a valid PSO

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // no-op
}
