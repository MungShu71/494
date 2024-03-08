import networkx as nx
import random
import matplotlib.pyplot as plt
G = nx.DiGraph()
H = nx.path_graph(6)
G.add_nodes_from(H)
G.add_edges_from(H.edges)
# nx.draw_networkx_nodes(G,pos)
# pos = nx.spring_layout(G)  # positions for all nodes
pos = nx.spring_layout(G, seed=47)  # positions for all nodes
# nx.draw_networkx_nodes(G,pos)



# G=nx.convert_node_labels_to_integers(G,pos)
print(G)


for n in range(1):
    # edge = []
    edge = random.sample(range(2,5), 3)
    # edge = random.sample(range(2,9), random.randint(2))
    nx.add_cycle(G, edge)

# print(G.edges(data=True))
for i in (nx.kosaraju_strongly_connected_components(G)):
    print(i)


largest = max(nx.kosaraju_strongly_connected_components(G), key=len)

print("largest: ", largest)

# print("____________")

# nx.draw(G)
labels = {}


for i in range(6):
    labels[i] = i
nx.draw_networkx_labels(G, pos,labels, font_size=10, font_color="whitesmoke")
nx.draw(G,pos)
# nx.draw(G,pos, labels,font_size=22, font_color="whitesmoke")

plt.axis("off")
plt.show()