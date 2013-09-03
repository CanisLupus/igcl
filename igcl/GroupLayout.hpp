#ifndef GROUPLAYOUT_HPP_
#define GROUPLAYOUT_HPP_

#include <map>
#include <set>
#include <vector>


class GroupLayout
{
	typedef unsigned int uint;

	class Sources		// auxiliary class that implements "to" and allows calls like layout.from().to()
	{
		friend class GroupLayout;

	private:
		GroupLayout * const layout;
		std::vector<int> sources;

	public:
		template <typename T, typename... Args>
		void to(T target, Args... targets) const
		{
			for (int source : sources) {
				layout->add(source, target);
			}
			to(targets...);	// recursive call to process connections to next target
		}

	private:
		template <typename... Args>
		Sources(GroupLayout * const layout, Args... sources) : layout(layout), sources{int(sources)...} {}

		void to() const;
	};

private:
	bool freeformed, allConnected;
	std::set<int> nodes;
	std::map<int, std::set<int> > prev, next;

public:
	GroupLayout();

	bool areConnected(int source, int target);
	//bool isNextOf(int source, int target) const;
	//bool isPreviousOf(int source, int target) const;
	const std::vector<int> getNextOf(int source) const;
	const std::vector<int> getPreviousOf(int target) const;
	void addNode(int id);
	void removeNode(int id);
	bool isFreeformed() const;

	uint size() const;
	void print() const;

	template <typename... Args>
	const Sources from(Args... sources)
	{
		return Sources(this, int(sources)...);
	}

	static const GroupLayout getFreeAllToAllLayout();
	static const GroupLayout getFreeMasterWorkersLayout();

	static const GroupLayout getAllToAllLayout(const uint nNodes);
	static const GroupLayout getMasterWorkersLayout(const uint nNodes);
	static const GroupLayout getTreeLayout(const uint nNodes, const uint cardinality);
	static const GroupLayout getSortTreeLayout(const uint nNodes, const uint cardinality);
	static const GroupLayout getRingLayout(const uint nNodes);

	template <typename T, typename... Args>
	static const GroupLayout getPipelineLayout(T nNodesOfSection, Args... sections)
	{
		GroupLayout layout;
		for (int node = 1; node <= nNodesOfSection; ++node) {
			layout.add(0, node);
		}
		getPipelineLayoutAux(layout, 1, nNodesOfSection, sections...);
		return layout;
	}

private:
	bool add(int source, int target);

	template <typename T, typename... Args>
	static void getPipelineLayoutAux(GroupLayout & layout, int currentNode, T nNodesOfSection, T nNodesOfNextSection, Args... nNodesOfLastSections)
	{
		for (int i = 0; i < nNodesOfSection; ++i)
			for (int j = 0; j < nNodesOfNextSection; ++j)
				layout.add(currentNode+i, currentNode+nNodesOfSection+j);	// connect the two

		getPipelineLayoutAux(layout, currentNode+nNodesOfSection, nNodesOfNextSection, nNodesOfLastSections...);	// recursive call
	}

	template <typename T>
	static void getPipelineLayoutAux(const GroupLayout & layout, int currentNode, T nNodesOfSection)
	{
	}
};


#endif /* GROUPLAYOUT_HPP_ */
