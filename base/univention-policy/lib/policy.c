/*
 * Univention Policy
 *  C source of the univnetion policy libary
 *
 * Copyright 2003-2011 Univention GmbH
 *
 * http://www.univention.de/
 *
 * All rights reserved.
 *
 * The source code of this program is made available
 * under the terms of the GNU Affero General Public License version 3
 * (GNU AGPL V3) as published by the Free Software Foundation.
 *
 * Binary versions of this program provided by Univention to you as
 * well as other copyrighted, protected or trademarked materials like
 * Logos, graphics, fonts, specific documentations and configurations,
 * cryptographic keys etc. are subject to a license agreement between
 * you and Univention and not subject to the GNU AGPL V3.
 *
 * In the case you use this program under the terms of the GNU AGPL V3,
 * the program is provided in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License with the Debian GNU/Linux or Univention distribution in file
 * /usr/share/common-licenses/AGPL-3; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <ldap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <univention/debug.h>
#include <univention/policy.h>

/*
 * returns parent dn of dn, NULL if dn doesn't have any parents
 */
static const char *parent_dn(const char *dn)
{
	char* pdn = strchr(dn, ',');
	if (pdn != NULL)
		++pdn;
	return pdn;
}

/*
 * returns object from list if it already exists, create new object otherwise
 */
struct univention_policy_list_s* univention_policy_list_get(struct univention_policy_list_s** list, char* name)
{
	struct univention_policy_list_s *new;
	struct univention_policy_list_s *cur;

	for (cur = *list; cur != NULL; cur = cur->next) {
		if (strcmp(cur->name, name) == 0)
			return cur;
	}

	/* policy not found: create new object */
	univention_debug(UV_DEBUG_POLICY, UV_DEBUG_ALL, "policy entry not found, creating new one");
	if ((new = malloc(sizeof(struct univention_policy_list_s))) == NULL)
		return NULL;
	new->name = strdup(name);
	new->attributes = NULL;

	if (*list == NULL)
		new->next = NULL;
	else
		new->next = *list;
	*list = new;

	return new;
}

/*
 * returns object from list if it already exists, create new object otherwise
 */
struct univention_policy_attribute_list_s* univention_policy_attribute_list_get(struct univention_policy_attribute_list_s** list, char* name)
{
	struct univention_policy_attribute_list_s *new;
	struct univention_policy_attribute_list_s *cur;

	for (cur = *list; cur != NULL; cur = cur->next) {
		if (strcmp(cur->name, name) == 0)
			return cur;
	}

	/* policy not found: create new object */
	univention_debug(UV_DEBUG_POLICY, UV_DEBUG_ALL, "attribute entry not found, creating new one");
	if ((new = malloc(sizeof(struct univention_policy_attribute_list_s))) == NULL)
		return NULL;
	new->name = strdup(name);
	new->values = NULL;

	if (*list == NULL)
		new->next = NULL;
	else
		new->next = *list;
	*list = new;

	return new;
}

void univention_policy_attribute_list_remove(struct univention_policy_attribute_list_s** list, char *name)
{
	struct univention_policy_attribute_list_s *cur, *prev;

	for (prev = NULL, cur = *list; cur != NULL; prev = cur, cur = cur->next) {
		if (strcmp(cur->name, name) == 0) {
			int i;

			if (prev == NULL) {
					*list = cur->next;
			} else {
					prev->next = cur->next;
			}

			free(cur->values->policy_dn);
			for (i = 0; cur->values->values != NULL && cur->values->values[i] != NULL; i++) {
				free(cur->values->values[i]);
			}
			free(cur->values->values);
			free(cur->values);

			return;
		}
	}
}

/** Check if object_classes contains object_class. */
static bool in_string_array(char **object_classes, const char *object_class)
{
	int i;
	if (object_classes != NULL)
		for (i = 0; object_classes[i] != NULL; i++)
			if (strcmp(object_classes[i], object_class) == 0)
				return true;
	return false;
}

static void print_string_array(char **object_classes)
{
	int i;
	for (i = 0; object_classes[i] != NULL; i++)
		printf("%s\n", object_classes[i]);
}

void univention_policy_cleanup(univention_policy_handle_t* handle)
{
	struct univention_policy_list_s* policy;
	for (policy = handle->policies; policy != NULL; policy = policy->next) {
		struct univention_policy_attribute_list_s *cur, *prev;
		for (prev = NULL, cur = policy->attributes; cur != NULL; prev = cur, cur = cur->next) {
			/* entry is empty, remove */
			if (cur->values == NULL || cur->values->values == NULL) {
				if (prev == NULL) {
					policy->attributes = cur->next;
				} else {
					prev->next = cur->next;
				}
				free(cur->values);
			}
		}
	}
}

void univention_policy_merge(LDAP* ld, char *dn, univention_policy_handle_t* handle, char** object_classes)
{
	int		rc;
	LDAPMessage	*res;
	struct  timeval	timeout;
	LDAPMessage	*e;
	BerElement	*ber;
	char		*a;
	struct berval		**vals;
	int		count;

	univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "considering policy: %s", dn);

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	if (( rc = ldap_search_ext_s( ld, dn, LDAP_SCOPE_BASE, "(objectClass=univentionPolicy)", NULL, 0, NULL, NULL, &timeout, 0, &res )) != LDAP_SUCCESS ) {
		if ( rc == LDAP_NO_SUCH_OBJECT ) {
			univention_debug(UV_DEBUG_LDAP, UV_DEBUG_WARN, "Not found");
		} else {
			univention_debug(UV_DEBUG_LDAP, UV_DEBUG_WARN, "ldap_search_ext_s returned ERROR");
		}
	}
	if ((count = ldap_count_entries( ld, res )) > 0) {
		univention_debug(UV_DEBUG_LDAP, UV_DEBUG_INFO, "count = %d", count);
		for ( e = ldap_first_entry( ld, res ); e != NULL; e = ldap_next_entry( ld, e ) ) {
			char *l_dn;
			struct univention_policy_list_s *policy = NULL;
			char **fixed_attributes = NULL;
			char **empty_attributes = NULL;
			bool apply = true;
			int i;

			if( ( l_dn = ldap_get_dn(ld, e) ) != NULL ) {
				univention_debug(UV_DEBUG_LDAP, UV_DEBUG_ALL, "DN: %s", l_dn);
				ldap_memfree( l_dn );
			}

			for (a = ldap_first_attribute(ld, e, &ber); a != NULL; a = ldap_next_attribute(ld, e, ber)) {
				if ( ( vals = ldap_get_values_len( ld, e, a ) ) != NULL ) {
					if (strcmp(a, "objectClass") == 0) {
						for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++)
							if (strcmp(vals[i]->bv_val, "top") != 0 && strcmp(vals[i]->bv_val, "univentionPolicyReference") != 0) {
								policy = univention_policy_list_get(&handle->policies, vals[i]->bv_val);
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "current policy type is %s", policy->name);
							}
					} else if (strcmp(a, "requiredObjectClasses") == 0) {
						for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++) {
							if (!in_string_array(object_classes, vals[i]->bv_val)) {
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "objectclass %s is required", vals[i]->bv_val);
								apply = false;
								break;
							}
						}
					} else if (strcmp(a, "prohibitedObjectClasses") == 0) {
						for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++) {
							if (in_string_array(object_classes, vals[i]->bv_val)) {
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "objectclass %s is prohibited", vals[i]->bv_val);
								apply = false;
								break;
							}
						}
					} else if (strcmp(a, "fixedAttributes") == 0 && fixed_attributes == NULL) {
						int count = ldap_count_values_len(vals);
						if ((fixed_attributes = calloc(count + 1, sizeof(char*))) == NULL)
							perror("calloc");
						for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++)
							fixed_attributes[i] = strdup(vals[i]->bv_val);
						fixed_attributes[i] = NULL;
					} else if (strcmp(a, "emptyAttributes") == 0 && empty_attributes == NULL) {
						int count = ldap_count_values_len(vals);
						if ((empty_attributes = calloc(count + 1, sizeof(char*))) == NULL)
							perror("calloc");
						for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++)
							empty_attributes[i] = strdup(vals[i]->bv_val);
						empty_attributes[i] = NULL;
					}
					ldap_value_free_len( vals );
				}
				ldap_memfree( a );
			}
			if ( ber != NULL ) {
				ber_free( ber, 0 );
			}

			if (apply) {
				univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "applying policy: %s", dn);

				/* clear attributes defined in emptyAttributes; empty value entries
				 * will be removed by _cleanup; they are necessary for now to mark that
				 * attribute has been set (even though empty) */
				for (i = 0; empty_attributes != NULL && empty_attributes[i] != NULL; i++) {
					//univention_policy_attribute_list_remove(&policy->attributes, empty_attributes[i]);
					struct univention_policy_attribute_list_s* attr;

					a = empty_attributes[i];
					univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "considering %s/%s", policy->name, a);
					attr = univention_policy_attribute_list_get(&policy->attributes, a);
					if (attr->values == NULL || in_string_array(fixed_attributes, a)) {
						if ((attr->values = malloc(sizeof(univention_policy_result_t))) == NULL)
							perror("malloc");
						attr->values->policy_dn = strdup(dn);
						attr->values->count = 0;
						attr->values->values = calloc(attr->values->count + 1, sizeof(char*));
					} else {
						univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "not setting attribute");
					}
				}

				for (a = ldap_first_attribute(ld, e, &ber); a != NULL; a = ldap_next_attribute(ld, e, ber)) {
					if ( ( vals = ldap_get_values_len( ld, e, a ) ) != NULL ) {
						if (policy != NULL && strcmp(a, "objectClass") != 0 && strcmp(a, "requiredObjectClasses") != 0 &&
								strcmp(a, "prohibitedObjectClasses") != 0 && strcmp(a, "fixedAttributes") != 0 &&
								strcmp(a, "emptyAttributes") != 0 && strcmp(a, "cn") != 0) {
							struct univention_policy_attribute_list_s* attr;
							univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "considering %s/%s", policy->name, a);
							attr = univention_policy_attribute_list_get(&policy->attributes, a);
							if (attr->values == NULL || in_string_array(fixed_attributes, a)) {

								if ((attr->values = malloc(sizeof(univention_policy_result_t))) == NULL)
									perror("malloc");

								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "setting attribute");
								attr->values->policy_dn = strdup(dn);
								attr->values->count = ldap_count_values_len(vals);
								attr->values->values = calloc(attr->values->count + 1, sizeof(char*));
								for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL); i++) {
									attr->values->values[i] = strdup(vals[i]->bv_val);
								}
								attr->values->values[i] = NULL;
							} else {
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "not setting attribute");
							}
						}
						ldap_value_free_len( vals );
					}
					ldap_memfree( a );
				}
				if ( ber != NULL ) {
					ber_free( ber, 0 );
				}
			}
		}
		ldap_memfree(e);
	}
	else {
		univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "No objectClass=univentionPolicyReference found");
	}

	ldap_msgfree( res );
	univention_debug(UV_DEBUG_LDAP, UV_DEBUG_ALL, "Search done.");
}

/*
 * reads policies for dn from conn
 */
univention_policy_handle_t* univention_policy_open(LDAP* ld, char* base, char* dn)
{
	const char* pdn;
	int rc;
	LDAPMessage *res;

	struct  timeval	timeout;
	LDAPMessage	*e;
	BerElement	*ber;
	char		*a;
	struct berval		**vals;
	int		i;
	int		count;
	char*		attrs[] = {"objectClass", "univentionPolicyReference", NULL};

	univention_policy_handle_t*	handle;
	char **object_classes = NULL;
	int policy_count = 0;
	char **policies = NULL;

	univention_debug(UV_DEBUG_LDAP, UV_DEBUG_INFO, "univention_policy_open with dn = %s", dn);
	if ((handle = malloc(sizeof(univention_policy_handle_t))) == NULL)
		return NULL;
	handle->policies = NULL;

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	for (pdn = dn; pdn != NULL; pdn = parent_dn(pdn)) {
		char* filter;

		univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "processing dn %s", pdn);

		if (pdn == dn)
			filter = "(objectClass=*)";
		else
			filter = "(objectClass=univentionPolicyReference)";

		if (( rc = ldap_search_ext_s( ld, pdn, LDAP_SCOPE_BASE, filter, attrs, 0, NULL, NULL, &timeout, 0, &res)) != LDAP_SUCCESS ) {
			if ( rc == LDAP_NO_SUCH_OBJECT ) {
				univention_debug(UV_DEBUG_LDAP, UV_DEBUG_WARN, "Not found");
			} else if ( rc != LDAP_SUCCESS ) {
				univention_debug(UV_DEBUG_LDAP, UV_DEBUG_ERROR,
						"%s: %s", pdn, ldap_err2string(rc));
				univention_policy_close(handle);
				return NULL;
			}
		}
		if ( (count = ldap_count_entries( ld, res )) > 0 ) {
			for ( e = ldap_first_entry( ld, res ); e != NULL; e = ldap_next_entry( ld, e ) ) {
				char *l_dn;

				if( ( l_dn = ldap_get_dn(ld, e) ) != NULL ) {
					univention_debug(UV_DEBUG_LDAP, UV_DEBUG_ALL, "DN: %s", l_dn);
					ldap_memfree( l_dn );
				}

				for (a = ldap_first_attribute(ld, e, &ber); a != NULL; a = ldap_next_attribute(ld, e, ber)) {
					if ( ( vals = ldap_get_values_len( ld, e, a ) ) != NULL ) {
						univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, " attibute: %s", a);
						if (strcmp(a, "objectClass") == 0 && pdn == dn) {
							int count;
							univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "get object classes for %s", pdn);
							count = ldap_count_values_len(vals);
							if ((object_classes = calloc(count + 1, sizeof(char*))) == NULL)
								perror("calloc");
							for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL) && i < count; i++) {
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "   object class: %s", vals[i]->bv_val);
								object_classes[i] = strdup(vals[i]->bv_val);
							}
							object_classes[i] = NULL;
						} else if (strcmp(a, "univentionPolicyReference") == 0 ) {
							univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "found policies for %s", pdn);
							policy_count = ldap_count_values_len(vals);
							if ((policies = calloc(policy_count + 1, sizeof(char*))) == NULL)
								perror("calloc");
							for (i = 0; (vals[i] != NULL && vals[i]->bv_val != NULL && i < policy_count); i++) {
								univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "   policy: %s", vals[i]->bv_val);
								policies[i] = strdup(vals[i]->bv_val);
							}
							policies[i] = NULL;
						}
						ldap_value_free_len( vals );
					}
					ldap_memfree( a );
				}
				univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "processing policies for %s", pdn);
				if (policies != NULL) {
					for (i = 0; policies[i] != NULL && i <= policy_count; i++) {
						univention_debug(UV_DEBUG_POLICY, UV_DEBUG_INFO, "%s", policies[i]);
						univention_policy_merge(ld, policies[i], handle, object_classes);
					}
				}
				if (ber != NULL) {
					ber_free(ber, 0);
				}
			}
			ldap_memfree(e);
		}

		ldap_msgfree( res );
		univention_debug(UV_DEBUG_LDAP, UV_DEBUG_ALL, "Search done.");

		if (strcmp(pdn, base) == 0)
			break;
	}

	univention_policy_cleanup(handle);

	if (object_classes != NULL) {
		for (i = 0; object_classes[i] != NULL; i++)
			free(object_classes[i]);
		free(object_classes);
		object_classes = NULL;
	}

	return handle;
}

/*
 * returns values for policy/attribute
 */
univention_policy_result_t* univention_policy_get(univention_policy_handle_t* handle, char* policy_name, char* attribute_name)
{
	struct univention_policy_list_s* policy;
	struct univention_policy_attribute_list_s* attribute;
	policy = univention_policy_list_get(&handle->policies, policy_name);
	attribute = univention_policy_attribute_list_get(&policy->attributes, attribute_name);
	return attribute->values;
}

/*
 * frees policy handle
 */
void univention_policy_close(univention_policy_handle_t* handle)
{
	struct univention_policy_list_s* policy;
	struct univention_policy_attribute_list_s* attribute;

	for (policy = handle->policies; policy != NULL; ) {
		struct univention_policy_list_s* current_policy;
		for (attribute = policy->attributes; attribute != NULL; ) {
			struct univention_policy_attribute_list_s* current_attribute;
			if (attribute->values != NULL) {
				if (attribute->values->values != NULL)
					free(attribute->values->values);
				if (attribute->values->policy_dn != NULL);
					free(attribute->values->policy_dn);
				free(attribute->values);
			}
			current_attribute = attribute;
			attribute = attribute->next;
			free(current_attribute);
		}
		current_policy = policy;
		policy = policy->next;
		free(current_policy);
	}
}
