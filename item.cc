#include "item.h"
#include "expr.h"

namespace ledger {

// jww (2004-07-21): If format.show_empty is set, then include all
// subaccounts, empty, balanced or no

item_t * walk_accounts(const account_t * account,
		       const node_t *    predicate,
		       const bool        show_subtotals)
{
  item_t * item = new item_t;
  item->account = account;

  std::time_t latest = 0;
  for (transactions_list::const_iterator i
	 = std::find_if(account->transactions.begin(),
			account->transactions.end(),
			value_predicate(predicate));
       i != account->transactions.end();
       i = std::find_if(++i, account->transactions.end(),
			value_predicate(predicate))) {
    if (std::difftime(latest, (*i)->entry->date) < 0)
      latest = (*i)->entry->date;

    item->value += *(*i);
    if (show_subtotals)
      item->total += *(*i);
  }
  item->date = latest;

  for (accounts_map::const_iterator i = account->accounts.begin();
       i != account->accounts.end();
       i++) {
    item_t * subitem = walk_accounts((*i).second, predicate, show_subtotals);
    subitem->parent = item;

    if (std::difftime(item->date, subitem->date) < 0)
      item->date = subitem->date;

    if (show_subtotals)
      item->total += subitem->total;
    if (show_subtotals ? subitem->total : subitem->value)
      item->subitems.push_back(subitem);
  }

  return item;
}

static inline void sum_items(const item_t * top,
			     const bool     show_subtotals,
			     item_t *	    item)
{
  if (top->account == item->account) {
    item->value += top->value;
    if (show_subtotals)
      item->total += top->value;
  }

  for (items_deque::const_iterator i = top->subitems.begin();
       i != top->subitems.end();
       i++)
    sum_items(*i, show_subtotals, item);
}

item_t * walk_items(const item_t *    top,
		    const account_t * account,
		    const node_t *    predicate,
		    const bool	      show_subtotals)
{
  item_t * item = new item_t;
  item->account = account;

  sum_items(top, show_subtotals, item);

  for (accounts_map::const_iterator i = account->accounts.begin();
       i != account->accounts.end();
       i++) {
    item_t * subitem = walk_items(top, (*i).second, predicate, show_subtotals);
    subitem->parent = item;

    if (show_subtotals)
      item->total += subitem->total;

    if (show_subtotals ? subitem->total : subitem->value)
      item->subitems.push_back(subitem);
  }

  return item;
}

item_t * walk_entries(entries_list::const_iterator begin,
		      entries_list::const_iterator end,
		      const node_t * predicate,
		      const bool     show_related,
		      const bool     show_inverted)
{
  unsigned int	  count  = 0;
  item_t *	  result = NULL;
  value_predicate pred_obj(predicate);

  for (entries_list::const_iterator i = std::find_if(begin, end, pred_obj);
       i != end;
       i = std::find_if(++i, end, pred_obj)) {
    item_t * item = NULL;

    for (transactions_list::const_iterator j
	   = std::find_if((*i)->transactions.begin(),
			  (*i)->transactions.end(), pred_obj);
	 j != (*i)->transactions.end();
	 j = std::find_if(++j,
		transactions_list::const_iterator((*i)->transactions.end()),
		pred_obj)) {
      assert(*i == (*j)->entry);

      if (! item) {
	item = new item_t;
	item->index = count++;
	item->date  = (*i)->date;
	item->payee = (*i)->payee;
      }

      // If show_inverted is true, it implies show_related.
      if (! show_inverted) {
	item_t * subitem = new item_t;
	subitem->parent  = item;
	subitem->date    = item->date;
	subitem->payee   = item->payee;
	subitem->account = (*j)->account;
	subitem->value   = *(*j);
	item->subitems.push_back(subitem);
      }

      if (show_related)
	for (transactions_list::iterator k = (*i)->transactions.begin();
	     k != (*i)->transactions.end();
	     k++)
	  if (*k != *j && ! ((*k)->flags & TRANSACTION_VIRTUAL)) {
	    item_t * subitem = new item_t;
	    subitem->parent  = item;
	    subitem->date    = item->date;
	    subitem->payee   = item->payee;
	    subitem->account = (*k)->account;
	    subitem->value   = *(*k);
	    if (show_inverted)
	      subitem->value.negate();
	    item->subitems.push_back(subitem);
	  }
    }

    if (item) {
      if (! result)
	result = new item_t;
      item->parent = result;
      result->subitems.push_back(item);
    }
  }

  return result;
}

struct cmp_items {
  const node_t * sort_order;

  cmp_items(const node_t * _sort_order) : sort_order(_sort_order) {}

  bool operator()(const item_t * left, const item_t * right) const {
    assert(left);
    assert(right);
    assert(sort_order);
    return sort_order->compute(left) < sort_order->compute(right);
  }
};

void item_t::sort(const node_t * sort_order)
{
  std::sort(subitems.begin(), subitems.end(), cmp_items(sort_order));
}

} // namespace ledger
