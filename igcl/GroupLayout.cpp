#include "GroupLayout.hpp"

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <algorithm>
#include <queue>


GroupLayout::GroupLayout() : freeformed(false), allConnected(false)
{
}


void GroupLayout::Sources::to() const
{
}


bool GroupLayout::add(int source, int target)
{
	if (nodes.count(source) and next[source].count(target))
		return false;
	if (nodes.count(target) and prev[target].count(source))
		return false;

	if (!nodes.count(source))
		nodes.insert(source);
	if (!nodes.count(target))
		nodes.insert(target);

	next[source].insert(target);
	prev[target].insert(source);

	return true;
}


bool GroupLayout::areConnected(int source, int target)
{
	/*if (nodes.count(source) == 0 or nodes.count(target) == 0) {
		return false;
	}*/
	if (freeformed) {
		return (allConnected or source == 0 or target == 0);		// because coordinator (0) connects to all
	}
	return next[source].count(target) > 0 or next[target].count(source) > 0;
}


/*bool GroupLayout::isNextOf(int source, int target) const
{
	if (freeformed) {
		return (allConnected or source == 0);
	}
	auto it = next.find(source);
	return it != next.end() and it->second.count(target) > 0;
}


bool GroupLayout::isPreviousOf(int source, int target) const
{
	if (freeformed) {
		return (allConnected or target == 0);
	}
	auto it = prev.find(source);
	return it != prev.end() and it->second.count(target) > 0;
}*/


const std::vector<int> GroupLayout::getNextOf(int source) const
{
	if (freeformed) {
		if (allConnected or source == 0) {
			std::set<int> ns = nodes;
			ns.erase(source);
			return std::vector<int>(ns.begin(), ns.end());
		} else {
			return std::vector<int>();
		}
	}

	auto it = next.find(source);
	if (it != next.end())
		return std::vector<int>(it->second.begin(), it->second.end());
	else
		return std::vector<int>();
}


const std::vector<int> GroupLayout::getPreviousOf(int target) const
{
	if (freeformed) {
		if (allConnected) {
			std::set<int> ns = nodes;
			ns.erase(target);
			return std::vector<int>(ns.begin(), ns.end());
		} else if (target != 0) {
			std::vector<int> ns;
			ns.push_back(0);
			return ns;
		}
	}

	auto it = prev.find(target);
	if (it != prev.end())
		return std::vector<int>(it->second.begin(), it->second.end());
	else
		return std::vector<int>();
}


void GroupLayout::addNode(int id)
{
	if (freeformed) {
		nodes.insert(id);
	}
}


void GroupLayout::removeNode(int id)
{
	if (freeformed) {
		nodes.erase(id);
	}
}


bool GroupLayout::isFreeformed() const
{
	return freeformed;
}


void GroupLayout::print() const
{
	std::cout << "NEXT" << std::endl;
	for (auto pair : next) {
		std::cout << pair.first << ": ";
		for (int n : pair.second) {
			std::cout << n << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;

	std::cout << "PREVIOUS" << std::endl;
	for (auto pair : prev) {
		std::cout << pair.first << ": ";
		for (int n : pair.second) {
			std::cout << n << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}


uint GroupLayout::size() const
{
	return nodes.size();
}


const GroupLayout GroupLayout::getFreeAllToAllLayout()
{
	GroupLayout layout;
	layout.freeformed = true;
	layout.allConnected = true;
	return layout;
}

const GroupLayout GroupLayout::getFreeMasterWorkersLayout()
{
	GroupLayout layout;
	layout.freeformed = true;
	layout.allConnected = false;
	return layout;
}

const GroupLayout GroupLayout::getAllToAllLayout(const uint nNodes)
{
	GroupLayout layout;

	for (uint i = 0;  i < nNodes-1;  i++) {
		for (uint j = i+1;  j < nNodes;  j++) {
			layout.add(i, j);
			layout.add(j, i);
		}
	}

	if (nNodes == 1) {
		layout.nodes.insert(0);
	}

	return layout;
}

const GroupLayout GroupLayout::getMasterWorkersLayout(const uint nNodes)
{
	GroupLayout layout;

	for (uint i = 1;  i < nNodes;  i++) {
		layout.add(0, i);
	}

	if (nNodes == 1) {
		layout.nodes.insert(0);
	}

	return layout;
}

const GroupLayout GroupLayout::getTreeLayout(const uint nNodes, const uint cardinality)
{
	GroupLayout layout;
	std::queue<uint> q;
	q.push(0);
	uint node = 1;

	while (node < nNodes) {
		uint parent = q.front();
		q.pop();

		uint limit = std::min(nNodes, node+cardinality);
		while (node < limit) {
			layout.add(parent, node);
			q.push(node);
			++node;
		}
	}

	if (nNodes == 1) {
		layout.nodes.insert(0);
	}

	return layout;
}


const GroupLayout GroupLayout::getSortTreeLayout(const uint nNodes, const uint cardinality)
{
	GroupLayout layout;
	std::queue< uint > q;
	q.push(0);
	uint node = 1;

	while (node < nNodes)
	{
		uint parent = q.front();
		q.pop();

		q.push(parent);

		for (uint limit=node+cardinality-1; node < limit and node < nNodes; ++node)
		{
			layout.add(parent, node);
			q.push(node);
		}
	}

	if (nNodes == 1) {
		layout.nodes.insert(0);
	}

	return layout;
}


const GroupLayout GroupLayout::getRingLayout(const uint nNodes)
{
	GroupLayout layout;

	for (uint node=1; node < nNodes; ++node) {
		layout.add(node-1, node);
	}
	layout.add(nNodes-1, 0);

	return layout;
}
