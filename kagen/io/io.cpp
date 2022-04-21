#include "kagen/io/io.h"

#include <mpi.h>

#include "kagen/generator_config.h"
#include "kagen/io/buffered_writer.h"

namespace kagen {
namespace {
// First invalid node on the last PE is the number of nodes in the graph
SInt FindNumberOfGlobalNodes(const VertexRange vertex_range) {
    PEID size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    SInt first_invalid_node = vertex_range.second;
    MPI_Bcast(&first_invalid_node, 1, MPI_UNSIGNED_LONG_LONG, size - 1, MPI_COMM_WORLD);

    return first_invalid_node;
}

// Length of all edge lists is the number of edges in the graph
SInt FindNumberOfGlobalEdges(const EdgeList& edges) {
    SInt local_num_edges = edges.size();
    SInt global_num_edges;
    MPI_Allreduce(&local_num_edges, &global_num_edges, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    return global_num_edges;
}

void CreateFile(const std::string& filename) {
    BufferedTextOutput<>(tag::create, filename);
}
} // namespace

void WriteGraph(const PGeneratorConfig& config, EdgeList& edges, const VertexRange vertex_range) {
    switch (config.output_format) {
        case OutputFormat::EDGE_LIST:
            WriteEdgeList(config.output_file, !config.output_header, config.output_single_file, edges, vertex_range);
            break;

        case OutputFormat::BINARY_EDGE_LIST:
            WriteBinaryEdgeList(
                config.output_file, !config.output_header, config.output_single_file, edges, vertex_range);
            break;
    }
}

//
// Text edge list
//
namespace {
void AppendEdgeList(const std::string& filename, const EdgeList& edges) {
    BufferedTextOutput<> out(tag::append, filename);
    for (const auto& [from, to]: edges) {
        out.WriteString("e ").WriteInt(from + 1).WriteChar(' ').WriteInt(to + 1).WriteChar('\n').Flush();
    }
}

void AppendEdgeListHeader(
    const std::string& filename, const bool single_file, const EdgeList& edges, const VertexRange vertex_range) {
    const SInt number_of_nodes = FindNumberOfGlobalNodes(vertex_range);
    const SInt number_of_edges = single_file ? FindNumberOfGlobalEdges(edges) : edges.size();

    BufferedTextOutput<> out(tag::append, filename);
    out.WriteString("p ").WriteInt(number_of_nodes).WriteChar(' ').WriteInt(number_of_edges).WriteChar('\n').Flush();
}
} // namespace

void WriteEdgeList(
    const std::string& filename, const bool omit_header, const bool single_file, const EdgeList& edges,
    const VertexRange vertex_range) {
    PEID rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (single_file) {
        if (rank == ROOT) {
            CreateFile(filename);
            if (!omit_header) {
                AppendEdgeListHeader(filename, true, edges, vertex_range);
            }
        }

        for (PEID pe = 0; pe < size; ++pe) {
            if (pe == rank) {
                AppendEdgeList(filename, edges);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    } else {
        const std::string my_filename = filename + "_" + std::to_string(rank);
        CreateFile(my_filename);
        if (!omit_header) {
            AppendEdgeListHeader(my_filename, false, edges, vertex_range);
        }
        AppendEdgeList(my_filename, edges);
    }
}

//
// Binary edge list
//
namespace {
void AppendBinaryEdgeList(const std::string& filename, const EdgeList& edges) {
    FILE* fout = fopen(filename.c_str(), "ab");
    for (const auto& [from, to]: edges) {
        const SInt edge[2] = {from + 1, to + 1};
        fwrite(edge, sizeof(SInt), 2, fout);
    }
    fclose(fout);
}

void AppendBinaryEdgeListHeader(
    const std::string& filename, const bool single_file, const EdgeList& edges, const VertexRange vertex_range) {
    const SInt number_of_nodes = FindNumberOfGlobalNodes(vertex_range);
    const SInt number_of_edges = single_file ? FindNumberOfGlobalEdges(edges) : edges.size();

    FILE* fout = fopen(filename.c_str(), "ab");
    fwrite(&number_of_nodes, sizeof(SInt), 1, fout);
    fwrite(&number_of_edges, sizeof(SInt), 1, fout);
    fclose(fout);
}
} // namespace

void WriteBinaryEdgeList(
    const std::string& filename, const bool omit_header, const bool single_file, const EdgeList& edges,
    const VertexRange vertex_range) {
    PEID rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (single_file) {
        if (rank == ROOT) {
            CreateFile(filename);
            if (!omit_header) {
                AppendBinaryEdgeListHeader(filename, true, edges, vertex_range);
            }
        }

        for (PEID pe = 0; pe < size; ++pe) {
            if (pe == rank) {
                AppendBinaryEdgeList(filename, edges);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
    } else {
        const std::string my_filename = filename + "_" + std::to_string(rank);
        CreateFile(my_filename);
        if (!omit_header) {
            AppendBinaryEdgeListHeader(my_filename, false, edges, vertex_range);
        }
        AppendBinaryEdgeList(my_filename, edges);
    }
}
} // namespace kagen
