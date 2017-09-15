////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include "partial_sync_helper.hpp"

#include "list.hpp"
#include "object_store.hpp"
#include "shared_realm.hpp"
#include "impl/object_notifier.hpp"
#include "impl/realm_coordinator.hpp"

#include <realm/table.hpp>

namespace realm {

inline std::string matches_property_for_object(const std::string& name)
{
    return name + "_matches";
}

PartialSyncHelper::PartialSyncHelper(Realm* realm)
{
    // Get or create the `__ResultSets` table within the Realm.
    auto table_name = ObjectStore::table_name_for_object_type("__ResultSets");
    bool table_was_added = false;
    m_parent_realm = realm;

    realm->begin_transaction();
    auto table = realm->read_group().get_or_add_table(table_name, &table_was_added);
    REALM_ASSERT(!table->has_shared_type());
    m_result_sets_table = std::move(table);
    if (table_was_added) {
        // Set up the initial schema.
        m_common_schema = {
            table->add_column(DataType::type_String, "matches_property"),
            table->add_column(DataType::type_String, "query"),
            table->add_column(DataType::type_Int, "status"),
            table->add_column(DataType::type_String, "error_message"),
        };
        realm->commit_transaction();
    } else {
        realm->cancel_transaction();
        // Load the existing schema.
        m_common_schema = {
            table->get_column_index("matches_property"),
            table->get_column_index("query"),
            table->get_column_index("status"),
            table->get_column_index("error_message"),
        };
    }
}

void PartialSyncHelper::register_query(const std::string& object_class,
                                       const std::string& query,
                                       std::function<PartialSyncResultCallback> callback)
{
    size_t link_column = get_matches_column_idx_for_object_class(object_class);
    auto matches_name = matches_property_for_object(object_class);

    // Create a new __ResultSets object, and stick it in the Realm.
    m_parent_realm->begin_transaction();
    Row row = m_result_sets_table->get(m_result_sets_table->add_empty_row());
    auto link_view = row.get_linklist(link_column);
    row.set_int(m_common_schema.idx_status, 0);
    row.set_string(m_common_schema.idx_query, query);
    row.set_string(m_common_schema.idx_matches_property, matches_name);
    m_parent_realm->commit_transaction();

    // Observe the new object and notify listener when the results are complete (status != 0)
    auto notifier = std::make_shared<_impl::ObjectNotifier>(row, m_parent_realm->shared_from_this());
    auto notification_callback = [&,
                                  link_view=std::move(link_view),
                                  notifier=notifier,
                                  callback=std::move(callback)](CollectionChangeSet, std::exception_ptr error) {
        if (error) {
            callback(List(), error);
            notifier->unregister();
            return;
        }
        // Get the status.
        size_t status = row.get_int(m_common_schema.idx_status);
        if (status == 0) {
            // Still computing...
            return;
        } else if (status == 1) {
            // Finished successfully.
            callback(List(m_parent_realm->shared_from_this(), std::move(link_view)), nullptr);
        } else {
            // Finished with error.
            std::string message = row.get_string(m_common_schema.idx_error_message);
            callback(List(), std::make_exception_ptr(std::runtime_error(std::move(message))));
        }
        notifier->unregister();
    };
    _impl::RealmCoordinator::register_notifier(notifier);
    notifier->add_callback(std::move(notification_callback));
}

size_t PartialSyncHelper::get_matches_column_idx_for_object_class(const std::string& object_class)
{
    auto it = m_object_type_schema.find(object_class);
    size_t idx = npos;
    if (it == m_object_type_schema.end()) {
        // Look up the schema in the table.
        auto raw_name = matches_property_for_object(object_class);
        m_parent_realm->begin_transaction();
        idx = m_result_sets_table->get_column_index(raw_name);
        // If it's not there, add a new column.
        if (idx == npos) {
            TableRef target_table = ObjectStore::table_for_object_type(m_parent_realm->read_group(), object_class);
            idx = m_result_sets_table->add_column_link(DataType::type_LinkList, raw_name, *target_table);
            m_parent_realm->commit_transaction();
        } else {
            m_parent_realm->cancel_transaction();
        }
        REALM_ASSERT_DEBUG(idx != npos);
        m_object_type_schema[object_class] = idx;
        return idx;
    } else {
        // We previously registered the object class.
        return it->second;
    }
}

}
