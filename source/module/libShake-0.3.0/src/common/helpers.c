#include "helpers.h"
#include <stdlib.h>

/* Linked list */

listElement *listElementPrepend(listElement *head)
{
	listElement *newNode = malloc(sizeof(listElement));
	if(!newNode)
	{
		return head;
	}

	newNode->next = head;
	return newNode;

}

listElement *listElementDelete(listElement *head, listElement *toDelNode)
{
	listElement *prevNode = NULL;
	listElement *curNode = head;

	while(curNode)
	{
		if(curNode == toDelNode)
		{
			if(!prevNode)
			{
				head = curNode->next;
			}
			else
			{
				prevNode->next = curNode->next;
			}

			free(curNode);
			return head;
		}
		prevNode = curNode;
		curNode = curNode->next;
	}

	return head;
}

listElement *listElementDeleteAll(listElement *head)
{
	listElement *curNode = head;

	while(curNode)
	{
		listElement *toDelNode = curNode;
		curNode = curNode->next;
		free(toDelNode);
	}

	return NULL;
}

listElement *listElementGet(listElement *head, unsigned int id)
{
	listElement *curNode = head;
	int i = 0;

	while(curNode)
	{
		if (i == id)
			return curNode;

		curNode = curNode->next;
		++i;
	}

	return NULL;
}

unsigned int listLength(listElement *head)
{
	listElement *curNode = head;
	unsigned int count = 0;

	while (curNode)
	{
		curNode = curNode->next;
		++count;
	}

	return count;
}
